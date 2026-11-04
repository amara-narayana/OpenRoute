#pragma once

#include <string>
#include <vector>
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <ostream>

namespace openroute::networking {

enum class NatType {
    UNKNOWN,
    DIRECT_PUBLIC,          // Host has a direct public IP, no NAT translation
    FULL_CONE,              // 1:1 mapping, any remote IP can send to mapped port
    RESTRICTED_CONE,        // Mapped port preserved, remote IP must be contacted first
    PORT_RESTRICTED_CONE,   // Mapped port preserved, remote IP+port must be contacted first
    SYMMETRIC_NAT,          // External port changes per destination; direct hole punching fails
    BLOCKED                 // Outbound UDP blocked or no response from any STUN server
};

inline const char* NatTypeToString(NatType type) {
    switch (type) {
        case NatType::DIRECT_PUBLIC:        return "Direct (Public IP)";
        case NatType::FULL_CONE:            return "Full Cone NAT";
        case NatType::RESTRICTED_CONE:      return "Restricted Cone NAT";
        case NatType::PORT_RESTRICTED_CONE: return "Port-Restricted Cone NAT";
        case NatType::SYMMETRIC_NAT:        return "Symmetric NAT (Relay Required)";
        case NatType::BLOCKED:              return "UDP Blocked";
        case NatType::UNKNOWN:
        default:                            return "Unknown";
    }
}

inline std::ostream& operator<<(std::ostream& os, NatType type) {
    return os << NatTypeToString(type);
}

struct StunEndpoint {
    std::string ip;
    uint16_t port{0};

    bool IsValid() const {
        return !ip.empty() && port != 0;
    }
};

struct NatProbeResult {
    NatType type{NatType::UNKNOWN};
    std::string public_ip;
    uint16_t public_port{0};
    std::string local_ip;
    uint16_t local_port{0};
    std::chrono::milliseconds latency{0};
    bool supports_direct_p2p{false};
    std::string summary;
};

// RFC 5389 / RFC 8489 STUN protocol implementation
class NatProbe {
public:
    static constexpr uint32_t STUN_MAGIC_COOKIE = 0x2112A442;
    static constexpr uint16_t STUN_BINDING_REQUEST = 0x0001;
    static constexpr uint16_t STUN_BINDING_RESPONSE = 0x0101;
    static constexpr uint16_t ATTR_MAPPED_ADDRESS = 0x0001;
    static constexpr uint16_t ATTR_XOR_MAPPED_ADDRESS = 0x0020;

    NatProbe();
    ~NatProbe();

    // Builds a standard 20-byte STUN Binding Request packet
    static std::vector<uint8_t> BuildBindingRequest(const std::array<uint8_t, 12>& transaction_id);

    // Parses a STUN Binding Response and extracts the mapped endpoint
    static bool ParseBindingResponse(
        const uint8_t* buffer,
        size_t length,
        const std::array<uint8_t, 12>& expected_transaction_id,
        StunEndpoint& out_endpoint
    );

    // Single server probe
    std::optional<StunEndpoint> ProbeServer(
        const std::string& host,
        uint16_t port,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(1500)
    );

    // Full 2-server probe to classify NAT type and assess direct P2P viability
    NatProbeResult PerformFullProbe(
        const std::string& primary_host = "stun.l.google.com",
        uint16_t primary_port = 19302,
        const std::string& secondary_host = "stun1.l.google.com",
        uint16_t secondary_port = 19302,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(1500)
    );

    // Classifies NAT type from two observed mapped endpoints and local endpoint
    static NatType ClassifyNat(
        const std::string& local_ip,
        const std::optional<StunEndpoint>& ep1,
        const std::optional<StunEndpoint>& ep2
    );

private:
    bool wsa_initialized_{false};
};

} // namespace openroute::networking
