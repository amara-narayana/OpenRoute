#include "core/gateway/lan_isolation.hpp"
#include <sstream>

namespace openroute::gateway {

LanIsolationFilter::LanIsolationFilter() = default;

void LanIsolationFilter::SetTunnelGatewayIp(uint32_t ip) {
    tunnel_gw_ip_ = ip;
}

void LanIsolationFilter::SetAllowLanAccess(bool allow) {
    allow_lan_ = allow;
}

bool LanIsolationFilter::IsPrivateRfc1918(uint32_t ip) {
    // 10.0.0.0/8 (0x0A000000 - 0x0AFFFFFF)
    if ((ip & 0xFF000000) == 0x0A000000) return true;

    // 172.16.0.0/12 (0xAC100000 - 0xAC1FFFFF)
    if ((ip & 0xFFF00000) == 0xAC100000) return true;

    // 192.168.0.0/16 (0xC0A80000 - 0xC0A8FFFF)
    if ((ip & 0xFFFF0000) == 0xC0A80000) return true;

    // 169.254.0.0/16 (Link Local)
    if ((ip & 0xFFFF0000) == 0xA9FE0000) return true;

    return false;
}

FilterVerdict LanIsolationFilter::InspectOutbound(const uint8_t* packet, size_t len) const {
    if (!packet || len < 20) {
        return FilterVerdict::DropMalformed;
    }

    // Check IPv4 version
    uint8_t version = (packet[0] >> 4) & 0x0F;
    if (version != 4) {
        // Drop non-IPv4 for IPv4 tunnel
        return FilterVerdict::DropMalformed;
    }

    uint8_t ihl = (packet[0] & 0x0F) * 4;
    if (ihl < 20 || len < ihl) {
        return FilterVerdict::DropMalformed;
    }

    // Destination IP at offset 16 (4 bytes big-endian)
    uint32_t dest_ip = (static_cast<uint32_t>(packet[16]) << 24) |
                      (static_cast<uint32_t>(packet[17]) << 16) |
                      (static_cast<uint32_t>(packet[18]) << 8)  |
                       static_cast<uint32_t>(packet[19]);

    // Destination is the gateway tunnel IP itself (e.g. DNS query to 10.77.0.1)
    if (dest_ip == tunnel_gw_ip_) {
        return FilterVerdict::Allow;
    }

    // Loopback 127.0.0.0/8
    if ((dest_ip & 0xFF000000) == 0x7F000000) {
        return FilterVerdict::DropLoopback;
    }

    // Multicast 224.0.0.0/4
    if ((dest_ip & 0xF0000000) == 0xE0000000) {
        return FilterVerdict::DropMulticast;
    }

    // Check RFC 1918 Private LAN
    if (!allow_lan_ && IsPrivateRfc1918(dest_ip)) {
        return FilterVerdict::DropPrivateLan;
    }

    return FilterVerdict::Allow;
}

std::string LanIsolationFilter::IpToString(uint32_t ip) {
    std::ostringstream ss;
    ss << ((ip >> 24) & 0xFF) << "."
       << ((ip >> 16) & 0xFF) << "."
       << ((ip >> 8) & 0xFF) << "."
       << (ip & 0xFF);
    return ss.str();
}

uint32_t LanIsolationFilter::StringToIp(const std::string& ip_str) {
    unsigned int a = 0, b = 0, c = 0, d = 0;
    if (sscanf_s(ip_str.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
        return (a << 24) | (b << 16) | (c << 8) | d;
    }
    return 0;
}

} // namespace openroute::gateway
