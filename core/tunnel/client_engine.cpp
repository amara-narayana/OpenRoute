#include "core/tunnel/client_engine.hpp"
#include "core/common/logger.hpp"

#include <chrono>
#include <sstream>

namespace openroute::tunnel {

ClientEngine::ClientEngine() = default;

ClientEngine::~ClientEngine() {
    Disconnect();
}

bool ClientEngine::ParseInvitationUrl(const std::string& url,
                                     GatewayProfile& out_profile,
                                     std::string& out_token) {
    if (url.rfind("openroute://invite?", 0) != 0) {
        LOG_ERROR << "Invalid invitation URL scheme: " << url;
        return false;
    }

    std::string query = url.substr(19);
    std::istringstream ss(query);
    std::string pair;

    while (std::getline(ss, pair, '&')) {
        auto eq = pair.find('=');
        if (eq == std::string::npos) continue;

        std::string key = pair.substr(0, eq);
        std::string val = pair.substr(eq + 1);

        if (key == "token") out_token = val;
        else if (key == "gw") out_profile.gateway_id = val;
        else if (key == "name") out_profile.gateway_name = val;
        else if (key == "pub") out_profile.gateway_public_key_b64 = val;
        else if (key == "port") out_profile.endpoint_port = static_cast<uint16_t>(std::stoi(val));
        else if (key == "host") out_profile.endpoint_host = val;
    }

    if (out_profile.endpoint_host.empty()) {
        out_profile.endpoint_host = "127.0.0.1"; // default local
    }

    return !out_token.empty() && !out_profile.gateway_public_key_b64.empty();
}

bool ClientEngine::Configure(const GatewayProfile& profile) {
    profile_ = profile;

    auto gw_key = security::Crypto::KeyFromBase64(profile_.gateway_public_key_b64);
    if (!gw_key) {
        LOG_ERROR << "Invalid gateway public key base64 in profile";
        return false;
    }
    gateway_pubkey_ = *gw_key;

    if (!profile_.client_private_key_b64.empty()) {
        auto priv = security::Crypto::KeyFromBase64(profile_.client_private_key_b64);
        if (priv) {
            client_keypair_.private_key = *priv;
            client_keypair_.public_key = security::Crypto::DerivePublicKey(client_keypair_.private_key);
        }
    }

    static const security::KeyBuffer kZeroKey{};
    if (security::Crypto::ConstantTimeEqual(client_keypair_.public_key.data(), kZeroKey.data(), security::kKeySize)) {
        client_keypair_ = security::Crypto::GenerateKeyPair();
        profile_.client_public_key_b64 = security::Crypto::KeyToBase64(client_keypair_.public_key);
    }

    LOG_INFO << "Configured client with gateway: " << profile_.gateway_name
             << " (" << profile_.endpoint_host << ":" << profile_.endpoint_port << ")";
    return true;
}

bool ClientEngine::PerformHandshake() {
#if defined(_WIN32)
    if (udp_socket_ == INVALID_SOCKET) return false;

    PacketHandshakeInitiation init_pkt{};
    HandshakeState hs_state{};
    uint32_t local_idx = static_cast<uint32_t>(rand());

    if (!NoiseProtocol::CreateInitiation(client_keypair_, gateway_pubkey_, {}, local_idx, init_pkt, hs_state)) {
        LOG_ERROR << "Failed to create Handshake Initiation packet";
        return false;
    }

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(profile_.endpoint_port);
    inet_pton(AF_INET, profile_.endpoint_host.c_str(), &(dest.sin_addr));

    auto start_time = std::chrono::steady_clock::now();

    // Send Initiation
    int sent = sendto(udp_socket_, reinterpret_cast<const char*>(&init_pkt), sizeof(init_pkt), 0,
                      reinterpret_cast<sockaddr*>(&dest), sizeof(dest));
    if (sent != sizeof(init_pkt)) {
        LOG_ERROR << "Failed to send Handshake Initiation: " << WSAGetLastError();
        return false;
    }

    // Set receive timeout for response (2 seconds)
    DWORD timeout = 2000;
    setsockopt(udp_socket_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

    PacketHandshakeResponse resp_pkt{};
    sockaddr_in from{};
    int from_len = sizeof(from);

    int recvd = recvfrom(udp_socket_, reinterpret_cast<char*>(&resp_pkt), sizeof(resp_pkt), 0,
                         reinterpret_cast<sockaddr*>(&from), &from_len);

    if (recvd != sizeof(resp_pkt)) {
        LOG_WARN << "Handshake timed out or received unexpected size (" << recvd << " bytes)";
        return false;
    }

    std::lock_guard<std::mutex> lock(session_mutex_);
    if (!NoiseProtocol::ConsumeResponse(client_keypair_, {}, resp_pkt, hs_state, transport_keys_)) {
        LOG_ERROR << "Failed to consume Handshake Response from gateway";
        return false;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - start_time).count();
    rtt_ms_ = elapsed / 1000.0;
    tx_counter_ = 0;

    LOG_INFO << "Handshake succeeded! Established session (RTT: " << rtt_ms_ << " ms)";
    return true;
#else
    return false;
#endif
}

bool ClientEngine::Connect() {
    if (state_ == ClientState::Connected) return true;

    state_ = ClientState::Connecting;
    LOG_INFO << "Connecting to gateway: " << profile_.gateway_name << "...";

#if defined(_WIN32)
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);

    udp_socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_socket_ == INVALID_SOCKET) {
        state_ = ClientState::Failed;
        return false;
    }

    sockaddr_in local_addr{};
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = 0; // ephemeral port
    bind(udp_socket_, reinterpret_cast<sockaddr*>(&local_addr), sizeof(local_addr));
#endif

    if (!PerformHandshake()) {
        state_ = ClientState::Failed;
        if (profile_.enable_kill_switch) {
            ApplyFailClosedPolicy(true);
        }
        return false;
    }

    state_ = ClientState::Connected;
    ApplyFailClosedPolicy(false);

    is_running_ = true;
    worker_thread_ = std::thread(&ClientEngine::BackgroundLoop, this);
    return true;
}

void ClientEngine::Disconnect() {
    if (state_ == ClientState::Disconnected) return;

    LOG_INFO << "Disconnecting from gateway...";
    is_running_ = false;

#if defined(_WIN32)
    if (udp_socket_ != INVALID_SOCKET) {
        closesocket(udp_socket_);
        udp_socket_ = INVALID_SOCKET;
    }
#endif

    tun_adapter_.Close();

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    state_ = ClientState::Disconnected;
    if (profile_.enable_kill_switch) {
        ApplyFailClosedPolicy(true); // Remain fail-closed if kill-switch configured
    }

    LOG_INFO << "Client disconnected.";
}

void ClientEngine::ApplyFailClosedPolicy(bool enable) {
    kill_switch_active_ = enable;
    if (enable) {
        LOG_INFO << "Fail-closed kill-switch ACTIVATED. Outbound traffic blocked to prevent leaks.";
    } else {
        LOG_INFO << "Fail-closed kill-switch DEACTIVATED. Normal routing restored.";
    }
}

bool ClientEngine::SimulateSendPacket(const uint8_t* ip_packet, size_t len,
                                      std::vector<uint8_t>& out_udp_packet) {
    std::lock_guard<std::mutex> lock(session_mutex_);
    if (!NoiseProtocol::EncryptTransport(transport_keys_, tx_counter_++, ip_packet, len, out_udp_packet)) {
        return false;
    }
    tx_bytes_ += len;
    return true;
}

bool ClientEngine::SimulateReceiveUdp(const uint8_t* udp_data, size_t len,
                                     std::vector<uint8_t>& out_decrypted_ip) {
    std::lock_guard<std::mutex> lock(session_mutex_);
    uint64_t counter = 0;
    if (!NoiseProtocol::DecryptTransport(transport_keys_, udp_data, len, counter, out_decrypted_ip)) {
        return false;
    }
    rx_bytes_ += out_decrypted_ip.size();
    return true;
}

void ClientEngine::BackgroundLoop() {
    auto last_keepalive = std::chrono::steady_clock::now();

    while (is_running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (!is_running_) break;

        auto now = std::chrono::steady_clock::now();
        if (now - last_keepalive >= std::chrono::seconds(25)) {
            // Send keepalive (Type 4 with 0-byte payload)
            std::vector<uint8_t> keepalive_pkt;
            if (SimulateSendPacket(nullptr, 0, keepalive_pkt)) {
#if defined(_WIN32)
                if (udp_socket_ != INVALID_SOCKET) {
                    sockaddr_in dest{};
                    dest.sin_family = AF_INET;
                    dest.sin_port = htons(profile_.endpoint_port);
                    inet_pton(AF_INET, profile_.endpoint_host.c_str(), &(dest.sin_addr));

                    sendto(udp_socket_, reinterpret_cast<const char*>(keepalive_pkt.data()),
                           static_cast<int>(keepalive_pkt.size()), 0,
                           reinterpret_cast<sockaddr*>(&dest), sizeof(dest));
                }
#endif
            }
            last_keepalive = now;
        }
    }
}

} // namespace openroute::tunnel
