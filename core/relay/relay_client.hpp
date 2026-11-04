#pragma once

#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <chrono>
#include <functional>
#include <mutex>
#include <ostream>

namespace openroute::relay {

enum class RelayState {
    DISCONNECTED,
    CONNECTING,
    REGISTERED,
    RELAYING,
    FAILED
};

inline const char* RelayStateToString(RelayState state) {
    switch (state) {
        case RelayState::DISCONNECTED: return "Disconnected";
        case RelayState::CONNECTING:   return "Connecting";
        case RelayState::REGISTERED:   return "Registered";
        case RelayState::RELAYING:     return "Relaying";
        case RelayState::FAILED:       return "Failed";
        default:                       return "Unknown";
    }
}

inline std::ostream& operator<<(std::ostream& os, RelayState state) {
    return os << RelayStateToString(state);
}

enum class RelayCommand : uint8_t {
    REGISTER    = 0x01,
    DATA        = 0x02,
    KEEPALIVE   = 0x03,
    DISCONNECT  = 0x04
};

#pragma pack(push, 1)
struct RelayHeader {
    uint32_t magic;         // 0x4F50524C ("OPRL")
    uint8_t  version;       // 0x01
    uint8_t  command;       // RelayCommand
    uint8_t  dest_key[32];  // 32-byte recipient public key
    uint16_t payload_len;   // Big-endian payload size
};
#pragma pack(pop)

struct RelayStats {
    uint64_t bytes_relayed_tx{0};
    uint64_t bytes_relayed_rx{0};
    uint64_t packets_relayed_tx{0};
    uint64_t packets_relayed_rx{0};
    std::chrono::milliseconds rtt{0};
};

using RelayPacketHandler = std::function<void(const std::array<uint8_t, 32>& src_key, const uint8_t* data, size_t len)>;

class RelayClient {
public:
    static constexpr uint32_t RELAY_MAGIC = 0x4F50524C; // "OPRL"
    static constexpr uint8_t RELAY_VERSION = 0x01;

    RelayClient(
        const std::string& relay_host,
        uint16_t relay_port,
        const std::array<uint8_t, 32>& my_public_key
    );
    ~RelayClient();

    // Builds a framed relay packet for sending via relay
    static std::vector<uint8_t> FramePacket(
        RelayCommand cmd,
        const std::array<uint8_t, 32>& dest_key,
        const uint8_t* payload,
        size_t payload_len
    );

    // Parses a framed relay packet received from relay
    static bool UnframePacket(
        const uint8_t* buffer,
        size_t length,
        RelayHeader& out_header,
        const uint8_t*& out_payload,
        size_t& out_payload_len
    );

    // Lifecycle
    bool Connect();
    void Disconnect();
    bool IsConnected() const;

    // Send encapsulated WireGuard transport packet to peer via relay
    bool SendRelayedPacket(const std::array<uint8_t, 32>& dest_peer_key, const uint8_t* data, size_t len);

    // Register callback for packets arriving via relay
    void SetPacketHandler(RelayPacketHandler handler);

    RelayState GetState() const;
    RelayStats GetStats() const;

private:
    std::string relay_host_;
    uint16_t relay_port_{0};
    std::array<uint8_t, 32> my_public_key_{};
    RelayState state_{RelayState::DISCONNECTED};
    RelayStats stats_{};
    mutable std::mutex state_mutex_;
    RelayPacketHandler packet_handler_;
};

} // namespace openroute::relay
