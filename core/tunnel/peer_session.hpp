#pragma once

#include "core/security/crypto.hpp"
#include "core/tunnel/noise_protocol.hpp"
#include "core/tunnel/replay_window.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace openroute::tunnel {

constexpr auto kRekeyAfterTime = std::chrono::seconds(120);
constexpr auto kKeepaliveInterval = std::chrono::seconds(25);
constexpr auto kRejectAfterTime = std::chrono::seconds(180);

enum class SessionState {
    Unauthenticated,
    Handshaking,
    Established,
    Expired
};

inline std::ostream& operator<<(std::ostream& os, SessionState s) {
    switch (s) {
        case SessionState::Unauthenticated: return os << "Unauthenticated";
        case SessionState::Handshaking:     return os << "Handshaking";
        case SessionState::Established:     return os << "Established";
        case SessionState::Expired:         return os << "Expired";
        default:                            return os << "Unknown";
    }
}

class PeerSession {
public:
    PeerSession(std::string peer_id,
                security::KeyBuffer public_key,
                std::string assigned_ip,
                security::KeyBuffer preshared_key = {});

    [[nodiscard]] const std::string& peer_id() const { return peer_id_; }
    [[nodiscard]] const security::KeyBuffer& public_key() const { return public_key_; }
    [[nodiscard]] const std::string& assigned_ip() const { return assigned_ip_; }
    [[nodiscard]] SessionState state() const;

    void SetState(SessionState s);
    void SetRemoteEndpoint(std::string endpoint);
    [[nodiscard]] std::string remote_endpoint() const;

    // Handshake integration
    void SetHandshakeState(HandshakeState state);
    std::optional<HandshakeState> GetHandshakeState();
    void CompleteHandshake(const TransportKeys& keys);

    // Transport packet encryption & decryption
    bool Encrypt(const uint8_t* plaintext, size_t len, std::vector<uint8_t>& out_packet);
    bool Decrypt(const uint8_t* packet_data, size_t len, std::vector<uint8_t>& out_plaintext);

    [[nodiscard]] bool NeedsRekey() const;
    [[nodiscard]] bool NeedsKeepalive() const;
    [[nodiscard]] bool IsExpired() const;

    [[nodiscard]] uint64_t tx_bytes() const { return tx_bytes_; }
    [[nodiscard]] uint64_t rx_bytes() const { return rx_bytes_; }
    [[nodiscard]] uint32_t local_index() const { return current_keys_.local_index; }
    [[nodiscard]] uint32_t remote_index() const { return current_keys_.remote_index; }

private:
    std::string peer_id_;
    security::KeyBuffer public_key_;
    std::string assigned_ip_;
    security::KeyBuffer preshared_key_;

    mutable std::mutex mutex_;
    SessionState state_ = SessionState::Unauthenticated;
    std::string remote_endpoint_;

    TransportKeys current_keys_{};
    std::optional<TransportKeys> previous_keys_{};
    std::optional<HandshakeState> pending_handshake_{};

    ReplayWindow replay_window_;
    uint64_t tx_counter_ = 0;
    uint64_t tx_bytes_ = 0;
    uint64_t rx_bytes_ = 0;

    std::chrono::steady_clock::time_point last_handshake_time_{};
    std::chrono::steady_clock::time_point last_tx_time_{};
    std::chrono::steady_clock::time_point last_rx_time_{};
};

} // namespace openroute::tunnel
