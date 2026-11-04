#include "core/tunnel/peer_session.hpp"
#include "core/common/logger.hpp"

namespace openroute::tunnel {

PeerSession::PeerSession(std::string peer_id,
                         security::KeyBuffer public_key,
                         std::string assigned_ip,
                         security::KeyBuffer preshared_key)
    : peer_id_(std::move(peer_id)),
      public_key_(public_key),
      assigned_ip_(std::move(assigned_ip)),
      preshared_key_(preshared_key) {}

SessionState PeerSession::state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

void PeerSession::SetState(SessionState s) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = s;
}

void PeerSession::SetRemoteEndpoint(std::string endpoint) {
    std::lock_guard<std::mutex> lock(mutex_);
    remote_endpoint_ = std::move(endpoint);
}

std::string PeerSession::remote_endpoint() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return remote_endpoint_;
}

void PeerSession::SetHandshakeState(HandshakeState state) {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_handshake_ = std::move(state);
    state_ = SessionState::Handshaking;
}

std::optional<HandshakeState> PeerSession::GetHandshakeState() {
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_handshake_;
}

void PeerSession::CompleteHandshake(const TransportKeys& keys) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == SessionState::Established) {
        previous_keys_ = current_keys_;
    }
    current_keys_ = keys;
    tx_counter_ = 0;
    replay_window_.Reset();
    pending_handshake_.reset();
    last_handshake_time_ = std::chrono::steady_clock::now();
    state_ = SessionState::Established;
}

bool PeerSession::Encrypt(const uint8_t* plaintext, size_t len, std::vector<uint8_t>& out_packet) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != SessionState::Established) {
        return false;
    }

    uint64_t counter = tx_counter_++;
    if (!NoiseProtocol::EncryptTransport(current_keys_, counter, plaintext, len, out_packet)) {
        return false;
    }

    tx_bytes_ += len;
    last_tx_time_ = std::chrono::steady_clock::now();
    return true;
}

bool PeerSession::Decrypt(const uint8_t* packet_data, size_t len, std::vector<uint8_t>& out_plaintext) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != SessionState::Established) {
        return false;
    }

    uint64_t counter = 0;
    // Try current keys first
    if (NoiseProtocol::DecryptTransport(current_keys_, packet_data, len, counter, out_plaintext)) {
        if (!replay_window_.ValidateAndMark(counter)) {
            LOG_WARN << "Anti-replay window rejected packet from peer " << peer_id_
                     << " counter=" << counter;
            out_plaintext.clear();
            return false;
        }
        rx_bytes_ += out_plaintext.size();
        last_rx_time_ = std::chrono::steady_clock::now();
        return true;
    }

    // If failed and previous keys exist (grace period during rekey), try previous keys
    if (previous_keys_.has_value()) {
        if (NoiseProtocol::DecryptTransport(*previous_keys_, packet_data, len, counter, out_plaintext)) {
            rx_bytes_ += out_plaintext.size();
            last_rx_time_ = std::chrono::steady_clock::now();
            return true;
        }
    }

    return false;
}

bool PeerSession::NeedsRekey() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != SessionState::Established) return false;
    auto elapsed = std::chrono::steady_clock::now() - last_handshake_time_;
    return elapsed >= kRekeyAfterTime || tx_counter_ >= 0xFFFFFFFFFFFFFFF0ULL;
}

bool PeerSession::NeedsKeepalive() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != SessionState::Established) return false;
    auto now = std::chrono::steady_clock::now();
    return (now - last_tx_time_) >= kKeepaliveInterval;
}

bool PeerSession::IsExpired() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != SessionState::Established) return false;
    auto elapsed = std::chrono::steady_clock::now() - last_handshake_time_;
    return elapsed >= kRejectAfterTime;
}

} // namespace openroute::tunnel
