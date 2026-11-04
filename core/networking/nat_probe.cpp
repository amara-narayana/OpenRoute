#include "core/networking/nat_probe.hpp"
#include "core/security/crypto.hpp"
#include "core/common/logger.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstring>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")

namespace openroute::networking {

NatProbe::NatProbe() {
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) == 0) {
        wsa_initialized_ = true;
    } else {
        LOG_ERROR << "[NatProbe] Failed to initialize Winsock";
    }
}

NatProbe::~NatProbe() {
    if (wsa_initialized_) {
        WSACleanup();
        wsa_initialized_ = false;
    }
}

std::vector<uint8_t> NatProbe::BuildBindingRequest(const std::array<uint8_t, 12>& transaction_id) {
    // STUN Header is 20 bytes:
    // 0..1:   Message Type (0x0001 = Binding Request)
    // 2..3:   Message Length (0x0000)
    // 4..7:   Magic Cookie (0x2112A442)
    // 8..19:  Transaction ID (96 bits / 12 bytes)
    std::vector<uint8_t> packet(20, 0);

    packet[0] = (STUN_BINDING_REQUEST >> 8) & 0xFF;
    packet[1] = STUN_BINDING_REQUEST & 0xFF;

    packet[2] = 0x00; // Length MSB
    packet[3] = 0x00; // Length LSB

    packet[4] = 0x21;
    packet[5] = 0x12;
    packet[6] = 0xA4;
    packet[7] = 0x42;

    std::memcpy(&packet[8], transaction_id.data(), 12);
    return packet;
}

bool NatProbe::ParseBindingResponse(
    const uint8_t* buffer,
    size_t length,
    const std::array<uint8_t, 12>& expected_transaction_id,
    StunEndpoint& out_endpoint
) {
    if (!buffer || length < 20) {
        return false;
    }

    // Verify Message Type (0x0101 = Binding Success Response)
    uint16_t msg_type = (static_cast<uint16_t>(buffer[0]) << 8) | buffer[1];
    if (msg_type != STUN_BINDING_RESPONSE) {
        return false;
    }

    uint16_t msg_length = (static_cast<uint16_t>(buffer[2]) << 8) | buffer[3];
    if (length < static_cast<size_t>(20 + msg_length)) {
        return false;
    }

    // Verify Magic Cookie (0x2112A442)
    uint32_t cookie = (static_cast<uint32_t>(buffer[4]) << 24) |
                      (static_cast<uint32_t>(buffer[5]) << 16) |
                      (static_cast<uint32_t>(buffer[6]) << 8) |
                      buffer[7];
    if (cookie != STUN_MAGIC_COOKIE) {
        return false;
    }

    // Verify Transaction ID
    if (std::memcmp(&buffer[8], expected_transaction_id.data(), 12) != 0) {
        return false;
    }

    // Iterate STUN attributes
    size_t offset = 20;
    size_t end = 20 + msg_length;

    while (offset + 4 <= end) {
        uint16_t attr_type = (static_cast<uint16_t>(buffer[offset]) << 8) | buffer[offset + 1];
        uint16_t attr_len = (static_cast<uint16_t>(buffer[offset + 2]) << 8) | buffer[offset + 3];
        offset += 4;

        if (offset + attr_len > end) {
            break;
        }

        if (attr_type == ATTR_XOR_MAPPED_ADDRESS && attr_len >= 8) {
            uint8_t family = buffer[offset + 1];
            if (family == 0x01) { // IPv4
                uint16_t raw_port = (static_cast<uint16_t>(buffer[offset + 2]) << 8) | buffer[offset + 3];
                uint16_t xor_port = raw_port ^ static_cast<uint16_t>(STUN_MAGIC_COOKIE >> 16);

                uint32_t raw_ip = (static_cast<uint32_t>(buffer[offset + 4]) << 24) |
                                  (static_cast<uint32_t>(buffer[offset + 5]) << 16) |
                                  (static_cast<uint32_t>(buffer[offset + 6]) << 8) |
                                  buffer[offset + 7];
                uint32_t xor_ip = raw_ip ^ STUN_MAGIC_COOKIE;

                char ip_str[INET_ADDRSTRLEN];
                struct in_addr in;
                in.s_addr = htonl(xor_ip);
                if (inet_ntop(AF_INET, &in, ip_str, sizeof(ip_str))) {
                    out_endpoint.ip = ip_str;
                    out_endpoint.port = xor_port;
                    return true;
                }
            }
        } else if (attr_type == ATTR_MAPPED_ADDRESS && attr_len >= 8) {
            uint8_t family = buffer[offset + 1];
            if (family == 0x01 && out_endpoint.ip.empty()) { // IPv4 fallback
                uint16_t raw_port = (static_cast<uint16_t>(buffer[offset + 2]) << 8) | buffer[offset + 3];
                char ip_str[INET_ADDRSTRLEN];
                struct in_addr in;
                std::memcpy(&in.s_addr, &buffer[offset + 4], 4);
                if (inet_ntop(AF_INET, &in, ip_str, sizeof(ip_str))) {
                    out_endpoint.ip = ip_str;
                    out_endpoint.port = raw_port;
                }
            }
        }

        // STUN attributes are padded to multiples of 4 bytes
        size_t padded_len = (attr_len + 3) & ~3;
        offset += padded_len;
    }

    return out_endpoint.IsValid();
}

std::optional<StunEndpoint> NatProbe::ProbeServer(
    const std::string& host,
    uint16_t port,
    std::chrono::milliseconds timeout
) {
    if (!wsa_initialized_) {
        return std::nullopt;
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        LOG_ERROR << "[NatProbe] Failed to create UDP socket";
        return std::nullopt;
    }

    DWORD timeout_ms = static_cast<DWORD>(timeout.count());
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    std::string port_str = std::to_string(port);
    if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0 || !res) {
        closesocket(sock);
        LOG_WARN << "[NatProbe] Failed to resolve STUN server: " << host;
        return std::nullopt;
    }

    std::array<uint8_t, 12> tx_id;
    security::Crypto::GenerateRandom(tx_id.data(), tx_id.size());

    auto req = BuildBindingRequest(tx_id);
    int sent = sendto(sock, reinterpret_cast<const char*>(req.data()), static_cast<int>(req.size()), 0,
                      res->ai_addr, static_cast<int>(res->ai_addrlen));
    freeaddrinfo(res);

    if (sent <= 0) {
        closesocket(sock);
        return std::nullopt;
    }

    uint8_t rx_buf[1024];
    sockaddr_in from_addr{};
    int from_len = sizeof(from_addr);

    int received = recvfrom(sock, reinterpret_cast<char*>(rx_buf), sizeof(rx_buf), 0,
                            reinterpret_cast<sockaddr*>(&from_addr), &from_len);
    closesocket(sock);

    if (received <= 0) {
        return std::nullopt;
    }

    StunEndpoint mapped_ep;
    if (ParseBindingResponse(rx_buf, static_cast<size_t>(received), tx_id, mapped_ep)) {
        return mapped_ep;
    }

    return std::nullopt;
}

NatType NatProbe::ClassifyNat(
    const std::string& local_ip,
    const std::optional<StunEndpoint>& ep1,
    const std::optional<StunEndpoint>& ep2
) {
    if (!ep1.has_value() && !ep2.has_value()) {
        return NatType::BLOCKED;
    }

    if (ep1.has_value() && !local_ip.empty() && ep1->ip == local_ip) {
        return NatType::DIRECT_PUBLIC;
    }

    if (ep1.has_value() && ep2.has_value()) {
        if (ep1->ip == ep2->ip && ep1->port == ep2->port) {
            // Mapped external port does not change across different target servers
            return NatType::FULL_CONE;
        } else if (ep1->ip == ep2->ip && ep1->port != ep2->port) {
            // Port changed when contacting a different server: Symmetric NAT
            return NatType::SYMMETRIC_NAT;
        }
    }

    if (ep1.has_value() || ep2.has_value()) {
        return NatType::RESTRICTED_CONE;
    }

    return NatType::UNKNOWN;
}

NatProbeResult NatProbe::PerformFullProbe(
    const std::string& primary_host,
    uint16_t primary_port,
    const std::string& secondary_host,
    uint16_t secondary_port,
    std::chrono::milliseconds timeout
) {
    NatProbeResult result;
    auto start_time = std::chrono::steady_clock::now();

    auto ep1 = ProbeServer(primary_host, primary_port, timeout);
    auto ep2 = ProbeServer(secondary_host, secondary_port, timeout);

    auto end_time = std::chrono::steady_clock::now();
    result.latency = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    result.type = ClassifyNat(result.local_ip, ep1, ep2);

    if (ep1.has_value()) {
        result.public_ip = ep1->ip;
        result.public_port = ep1->port;
    } else if (ep2.has_value()) {
        result.public_ip = ep2->ip;
        result.public_port = ep2->port;
    }

    result.supports_direct_p2p = (result.type == NatType::DIRECT_PUBLIC ||
                                  result.type == NatType::FULL_CONE ||
                                  result.type == NatType::RESTRICTED_CONE ||
                                  result.type == NatType::PORT_RESTRICTED_CONE);

    if (result.type == NatType::SYMMETRIC_NAT) {
        result.summary = "Symmetric NAT detected. Gateway will coordinate connections via authenticated relay.";
    } else if (result.supports_direct_p2p) {
        result.summary = "Direct P2P traversal supported. High-speed WireGuard tunnel ready.";
    } else if (result.type == NatType::BLOCKED) {
        result.summary = "UDP traffic appears filtered or offline. Direct connectivity unavailable.";
    } else {
        result.summary = "NAT status indeterminate. Probing fallback mechanisms.";
    }

    return result;
}

} // namespace openroute::networking
