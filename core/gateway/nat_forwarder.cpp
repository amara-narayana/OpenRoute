#include "core/gateway/nat_forwarder.hpp"
#include "core/common/logger.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <shlobj.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#endif

#include <array>
#include <cstdio>
#include <sstream>

namespace openroute::gateway {

NatForwarder::NatForwarder() = default;

NatForwarder::~NatForwarder() {
    if (configured_ && !nat_name_.empty()) {
        TeardownNat(nat_name_);
    }
}

bool NatForwarder::IsElevated() {
#if defined(_WIN32)
    BOOL is_admin = FALSE;
    PSID admin_group = nullptr;
    SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&nt_authority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                                &admin_group)) {
        CheckTokenMembership(nullptr, admin_group, &is_admin);
        FreeSid(admin_group);
    }
    return is_admin == TRUE;
#else
    return false;
#endif
}

std::vector<NetworkInterfaceInfo> NatForwarder::EnumerateInterfaces() {
    std::vector<NetworkInterfaceInfo> interfaces;
#if defined(_WIN32)
    ULONG buf_len = 15000;
    std::vector<BYTE> buffer(buf_len);
    auto* addrs = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());

    ULONG flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG ret = GetAdaptersAddresses(AF_INET, flags, nullptr, addrs, &buf_len);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(buf_len);
        addrs = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        ret = GetAdaptersAddresses(AF_INET, flags, nullptr, addrs, &buf_len);
    }

    if (ret == NO_ERROR) {
        for (auto* curr = addrs; curr != nullptr; curr = curr->Next) {
            NetworkInterfaceInfo info;

            // Convert wide strings to string
            char name_buf[256] = {0};
            WideCharToMultiByte(CP_UTF8, 0, curr->FriendlyName, -1, name_buf, static_cast<int>(sizeof(name_buf)), nullptr, nullptr);
            info.alias = name_buf;

            char desc_buf[256] = {0};
            WideCharToMultiByte(CP_UTF8, 0, curr->Description, -1, desc_buf, static_cast<int>(sizeof(desc_buf)), nullptr, nullptr);
            info.description = desc_buf;

            info.is_up = (curr->OperStatus == IfOperStatusUp);
            info.is_wireless = (curr->IfType == IF_TYPE_IEEE80211);

            // Extract IPv4 address
            if (curr->FirstUnicastAddress) {
                auto* sockaddr_in = reinterpret_cast<SOCKADDR_IN*>(curr->FirstUnicastAddress->Address.lpSockaddr);
                char ip_buf[INET_ADDRSTRLEN] = {0};
                inet_ntop(AF_INET, &(sockaddr_in->sin_addr), ip_buf, INET_ADDRSTRLEN);
                info.ipv4_address = ip_buf;
            }

            interfaces.push_back(info);
        }
    }
#endif
    return interfaces;
}

std::string NatForwarder::DetectPrimaryInterface() {
    auto ifaces = EnumerateInterfaces();
    // Prefer Up Wi-Fi or Ethernet
    for (const auto& iface : ifaces) {
        if (iface.is_up && (iface.is_wireless || iface.alias.find("Ethernet") != std::string::npos)) {
            if (!iface.ipv4_address.empty() && iface.ipv4_address != "127.0.0.1") {
                return iface.alias;
            }
        }
    }
    // Fallback to any UP interface with an IPv4 address
    for (const auto& iface : ifaces) {
        if (iface.is_up && !iface.ipv4_address.empty() && iface.ipv4_address != "127.0.0.1") {
            return iface.alias;
        }
    }
    return "Wi-Fi"; // default fallback
}

bool NatForwarder::ExecutePowerShellCommand(const std::string& cmd, std::string* output) {
    std::string full_cmd = "powershell -NoProfile -NonInteractive -Command \"" + cmd + "\"";
    FILE* pipe = _popen(full_cmd.c_str(), "r");
    if (!pipe) {
        LOG_ERROR << "Failed to spawn PowerShell process for: " << cmd;
        return false;
    }

    char buffer[256];
    std::ostringstream ss;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        ss << buffer;
    }

    int rc = _pclose(pipe);
    if (output) {
        *output = ss.str();
    }
    return rc == 0;
}

bool NatForwarder::SetupNat(const std::string& nat_name, const std::string& subnet_prefix) {
    if (!IsElevated()) {
        LOG_WARN << "Configuring Windows NetNat requires administrator privileges. Running in un-elevated mode.";
        nat_name_ = nat_name;
        configured_ = false;
        return false;
    }

    // Check if NetNat already exists
    std::string check_cmd = "Get-NetNat -Name '" + nat_name + "' -ErrorAction SilentlyContinue";
    std::string out;
    ExecutePowerShellCommand(check_cmd, &out);

    if (out.find(nat_name) == std::string::npos) {
        std::string create_cmd = "New-NetNat -Name '" + nat_name +
                                 "' -InternalIPInterfaceAddressPrefix '" + subnet_prefix + "'";
        if (!ExecutePowerShellCommand(create_cmd, &out)) {
            LOG_ERROR << "Failed to create NetNat for " << subnet_prefix << ": " << out;
            return false;
        }
    }

    nat_name_ = nat_name;
    configured_ = true;
    LOG_INFO << "Windows NetNat configured successfully: " << nat_name << " (" << subnet_prefix << ")";
    return true;
}

bool NatForwarder::EnableForwarding(const std::string& external_interface,
                                    const std::string& tunnel_interface) {
    if (!IsElevated()) {
        LOG_WARN << "Enabling IP forwarding requires administrator privileges.";
        return false;
    }

    std::string ext_cmd = "Set-NetIPInterface -InterfaceAlias '" + external_interface + "' -Forwarding Enabled";
    ExecutePowerShellCommand(ext_cmd);

    std::string tun_cmd = "Set-NetIPInterface -InterfaceAlias '" + tunnel_interface + "' -Forwarding Enabled -ErrorAction SilentlyContinue";
    ExecutePowerShellCommand(tun_cmd);

    LOG_INFO << "IP forwarding enabled on " << external_interface << " and " << tunnel_interface;
    return true;
}

bool NatForwarder::TeardownNat(const std::string& nat_name) {
    if (!IsElevated()) return false;

    std::string teardown_cmd = "Remove-NetNat -Name '" + nat_name + "' -Confirm:$false -ErrorAction SilentlyContinue";
    ExecutePowerShellCommand(teardown_cmd);
    configured_ = false;
    LOG_INFO << "NetNat " << nat_name << " removed.";
    return true;
}

} // namespace openroute::gateway
