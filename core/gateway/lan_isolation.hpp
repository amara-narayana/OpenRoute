#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

namespace openroute::gateway {

enum class FilterVerdict {
    Allow,
    DropPrivateLan,
    DropLoopback,
    DropMulticast,
    DropMalformed
};

inline const char* VerdictToString(FilterVerdict v) {
    switch (v) {
        case FilterVerdict::Allow: return "ALLOW";
        case FilterVerdict::DropPrivateLan: return "DROP_PRIVATE_LAN";
        case FilterVerdict::DropLoopback: return "DROP_LOOPBACK";
        case FilterVerdict::DropMulticast: return "DROP_MULTICAST";
        case FilterVerdict::DropMalformed: return "DROP_MALFORMED";
        default: return "DROP_UNKNOWN";
    }
}

inline std::ostream& operator<<(std::ostream& os, FilterVerdict v) {
    return os << VerdictToString(v);
}

class LanIsolationFilter {
public:
    LanIsolationFilter();

    void SetTunnelGatewayIp(uint32_t ip); // in host byte order (e.g. 0x0A4D0001 for 10.77.0.1)
    void SetAllowLanAccess(bool allow);

    // Inspects unencrypted IPv4 packet exiting the tunnel from a guest peer
    [[nodiscard]] FilterVerdict InspectOutbound(const uint8_t* packet, size_t len) const;

    static bool IsPrivateRfc1918(uint32_t ip);
    static std::string IpToString(uint32_t ip);
    static uint32_t StringToIp(const std::string& ip_str);

private:
    uint32_t tunnel_gw_ip_ = 0x0A4D0001; // 10.77.0.1
    bool allow_lan_ = false;
};

} // namespace openroute::gateway
