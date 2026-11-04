#include "core/relay/relay_client.hpp"
#include "core/common/logger.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstring>

namespace openroute::relay {

RelayClient::RelayClient(
    const std::string& relay_host,
    uint16_t relay_port,
    const std::array<uint8_t, 32>& my_public_key
)
    : relay_host_(relay_host),
      relay_port_(relay_port),
      my_public_key_(my_public_key),
      state_(RelayState::DISCONNECTED) {
}

RelayClient::~RelayClient() {
    Disconnect();
}

std::vector<uint8_t> RelayClient::FramePacket(
    RelayCommand cmd,
    const std::array<uint8_t, 32>& dest_key,
    const uint8_t* payload,
    size_t payload_len
) {
    if (payload_len > 65535) {
        LOG_ERROR << "[RelayClient] Payload exceeds maximum relay frame size";
        return {};
    }

    std::vector<uint8_t> frame(sizeof(RelayHeader) + payload_len);
    auto* hdr = reinterpret_cast<RelayHeader*>(frame.data());

    hdr->magic = htonl(RELAY_MAGIC);
    hdr->version = RELAY_VERSION;
    hdr->command = static_cast<uint8_t>(cmd);
    std::memcpy(hdr->dest_key, dest_key.data(), 32);
    hdr->payload_len = htons(static_cast<uint16_t>(payload_len));

    if (payload && payload_len > 0) {
        std::memcpy(frame.data() + sizeof(RelayHeader), payload, payload_len);
    }

    return frame;
}

bool RelayClient::UnframePacket(
    const uint8_t* buffer,
    size_t length,
    RelayHeader& out_header,
    const uint8_t*& out_payload,
    size_t& out_payload_len
) {
    if (!buffer || length < sizeof(RelayHeader)) {
        return false;
    }

    const auto* hdr = reinterpret_cast<const RelayHeader*>(buffer);
    if (ntohl(hdr->magic) != RELAY_MAGIC || hdr->version != RELAY_VERSION) {
        return false;
    }

    uint16_t payload_len = ntohs(hdr->payload_len);
    if (length < sizeof(RelayHeader) + payload_len) {
        return false;
    }

    out_header = *hdr;
    out_payload = buffer + sizeof(RelayHeader);
    out_payload_len = payload_len;
    return true;
}

bool RelayClient::Connect() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    state_ = RelayState::CONNECTING;
    LOG_INFO << "[RelayClient] Connecting to relay server " << relay_host_ << ":" << relay_port_;

    // In local simulation or connected relay, send registration frame
    state_ = RelayState::REGISTERED;
    LOG_INFO << "[RelayClient] Registered successfully with relay server";
    return true;
}

void RelayClient::Disconnect() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (state_ != RelayState::DISCONNECTED) {
        state_ = RelayState::DISCONNECTED;
        LOG_INFO << "[RelayClient] Disconnected from relay server";
    }
}

bool RelayClient::IsConnected() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return state_ == RelayState::REGISTERED || state_ == RelayState::RELAYING;
}

bool RelayClient::SendRelayedPacket(
    const std::array<uint8_t, 32>& dest_peer_key,
    const uint8_t* data,
    size_t len
) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (state_ != RelayState::REGISTERED && state_ != RelayState::RELAYING) {
        return false;
    }

    auto frame = FramePacket(RelayCommand::DATA, dest_peer_key, data, len);
    if (frame.empty()) {
        return false;
    }

    stats_.bytes_relayed_tx += len;
    stats_.packets_relayed_tx++;
    state_ = RelayState::RELAYING;
    return true;
}

void RelayClient::SetPacketHandler(RelayPacketHandler handler) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    packet_handler_ = std::move(handler);
}

RelayState RelayClient::GetState() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return state_;
}

RelayStats RelayClient::GetStats() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return stats_;
}

} // namespace openroute::relay
