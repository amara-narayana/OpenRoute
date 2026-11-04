#pragma once

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "core/security/crypto.hpp"
#include "core/tunnel/wireguard_types.hpp"
#include "core/tunnel/noise_protocol.hpp"
#include "core/tunnel/peer_session.hpp"
#include "core/tunnel/wintun_adapter.hpp"
#include "core/diagnostics/diagnostics_engine.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace openroute::tunnel {

enum class ClientState {
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
    Failed
};

inline const char* ClientStateToString(ClientState s) {
    switch (s) {
        case ClientState::Disconnected: return "DISCONNECTED";
        case ClientState::Connecting:   return "CONNECTING";
        case ClientState::Connected:    return "CONNECTED";
        case ClientState::Reconnecting: return "RECONNECTING";
        case ClientState::Failed:       return "FAILED";
        default:                        return "UNKNOWN";
    }
}

inline std::ostream& operator<<(std::ostream& os, ClientState s) {
    return os << ClientStateToString(s);
}

struct GatewayProfile {
    std::string gateway_id;
    std::string gateway_name;
    std::string endpoint_host;
    uint16_t endpoint_port = 51820;
    std::string gateway_public_key_b64;
    std::string client_assigned_ip = "10.77.0.2";
    std::string client_private_key_b64;
    std::string client_public_key_b64;
    bool enable_kill_switch = true;
};

class ClientEngine {
public:
    ClientEngine();
    ~ClientEngine();

    bool ParseInvitationUrl(const std::string& url, GatewayProfile& out_profile, std::string& out_token);

    bool Configure(const GatewayProfile& profile);
    bool Connect();
    void Disconnect();

    [[nodiscard]] ClientState state() const { return state_; }
    [[nodiscard]] const GatewayProfile& profile() const { return profile_; }

    [[nodiscard]] uint64_t tx_bytes() const { return tx_bytes_; }
    [[nodiscard]] uint64_t rx_bytes() const { return rx_bytes_; }
    [[nodiscard]] double rtt_ms() const { return rtt_ms_; }
    [[nodiscard]] bool is_kill_switch_active() const { return kill_switch_active_; }

    // Direct programmatic packet simulation for verification
    bool SimulateSendPacket(const uint8_t* ip_packet, size_t len, std::vector<uint8_t>& out_udp_packet);
    bool SimulateReceiveUdp(const uint8_t* udp_data, size_t len, std::vector<uint8_t>& out_decrypted_ip);

private:
    void BackgroundLoop();
    bool PerformHandshake();
    void ApplyFailClosedPolicy(bool enable);

    GatewayProfile profile_{};
    security::KeyPair client_keypair_{};
    security::KeyBuffer gateway_pubkey_{};

    std::atomic<ClientState> state_{ClientState::Disconnected};
    std::atomic<bool> is_running_{false};
    bool kill_switch_active_ = false;

    TransportKeys transport_keys_{};
    uint64_t tx_counter_ = 0;
    std::atomic<uint64_t> tx_bytes_{0};
    std::atomic<uint64_t> rx_bytes_{0};
    std::atomic<double> rtt_ms_{0.0};

    std::thread worker_thread_;
    mutable std::mutex session_mutex_;

#if defined(_WIN32)
    SOCKET udp_socket_ = INVALID_SOCKET;
#endif
    WintunAdapter tun_adapter_;
};

} // namespace openroute::tunnel
