#include "core/gateway/gateway_server.hpp"
#include "core/common/logger.hpp"

#include <chrono>
#include <cstring>

namespace openroute::gateway {

namespace {

inline uint32_t ExtractDestIp(const uint8_t* ip_packet, size_t len) {
    if (len < 20) return 0;
    return (static_cast<uint32_t>(ip_packet[16]) << 24) |
           (static_cast<uint32_t>(ip_packet[17]) << 16) |
           (static_cast<uint32_t>(ip_packet[18]) << 8)  |
            static_cast<uint32_t>(ip_packet[19]);
}

} // namespace

GatewayServer::GatewayServer(storage::Database& db, GatewayConfig config)
    : config_(std::move(config)),
      db_(db),
      device_mgr_(db),
      invitation_mgr_(db),
      quota_mgr_(&db) {}

GatewayServer::~GatewayServer() {
    Stop();
}

bool GatewayServer::Initialize() {
    LOG_INFO << "Initializing OpenRoute Gateway: " << config_.name;

    // 1. Check if gateway record exists in database
    try {
        auto stmt = db_.Prepare("SELECT public_key, encrypted_private_key FROM gateways WHERE id = ?1;");
        stmt.BindText(1, config_.id);
        if (stmt.Step()) {
            std::string pub_b64 = stmt.GetText(0);
            auto enc_priv = stmt.GetBlob(1);

            std::vector<uint8_t> dec_priv;
            if (security::Crypto::UnprotectData(enc_priv.data(), enc_priv.size(), dec_priv) &&
                dec_priv.size() == security::kKeySize) {
                std::memcpy(keypair_.private_key.data(), dec_priv.data(), security::kKeySize);
                keypair_.public_key = security::Crypto::DerivePublicKey(keypair_.private_key);
                LOG_INFO << "Loaded existing gateway keypair. Public key: " << pub_b64;
            }
        }
    } catch (...) {}

    // 2. Generate new keypair if none existed
    static const security::KeyBuffer kZeroKey{};
    if (security::Crypto::ConstantTimeEqual(keypair_.public_key.data(), kZeroKey.data(), security::kKeySize)) {
        keypair_ = security::Crypto::GenerateKeyPair();
        std::string pub_b64 = security::Crypto::KeyToBase64(keypair_.public_key);

        std::vector<uint8_t> enc_priv;
        security::Crypto::ProtectData(keypair_.private_key.data(), keypair_.private_key.size(), enc_priv);

        auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        auto insert_stmt = db_.Prepare(R"(
            INSERT OR REPLACE INTO gateways (id, name, public_key, encrypted_private_key, listen_port, tunnel_subnet, egress_interface, is_active, created_at, updated_at)
            VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, 1, ?8, ?9);
        )");
        insert_stmt.BindText(1, config_.id);
        insert_stmt.BindText(2, config_.name);
        insert_stmt.BindText(3, pub_b64);
        insert_stmt.BindBlob(4, enc_priv.data(), enc_priv.size());
        insert_stmt.BindInt(5, config_.listen_port);
        insert_stmt.BindText(6, config_.tunnel_subnet);
        insert_stmt.BindText(7, config_.egress_interface);
        insert_stmt.BindInt64(8, now);
        insert_stmt.BindInt64(9, now);
        insert_stmt.Step();

        LOG_INFO << "Created new persistent gateway identity: " << pub_b64;
    }

    // 3. Configure LAN Isolation Filter
    uint32_t gw_ip_int = LanIsolationFilter::StringToIp(config_.tunnel_ip);
    lan_filter_.SetTunnelGatewayIp(gw_ip_int);
    lan_filter_.SetAllowLanAccess(false); // Strict isolation by default

    // 4. Enroll approved devices from database
    auto devices = device_mgr_.ListDevices(config_.id);
    for (const auto& dev : devices) {
        if (dev.status == device::DeviceStatus::Approved) {
            auto pub_key = security::Crypto::KeyFromBase64(dev.public_key_b64);
            if (pub_key) {
                auto session = std::make_shared<tunnel::PeerSession>(dev.id, *pub_key, dev.assigned_ip);
                sessions_by_pubkey_[dev.public_key_b64] = session;
                sessions_by_ip_[dev.assigned_ip] = session;

                accounting::PeerAccountingState act{};
                act.device_id = dev.id;
                act.expires_at = dev.expires_at;
                quota_mgr_.RegisterPeer(act);

                shapers_[dev.id] = std::make_unique<bandwidth::PeerBandwidthShaper>();
            }
        }
    }

    LOG_INFO << "Enrolled " << sessions_by_pubkey_.size() << " approved device(s) into gateway cache.";
    return true;
}

bool GatewayServer::Start() {
    if (is_running_) return true;

    // Initialize UDP socket
#if defined(_WIN32)
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        LOG_ERROR << "WSAStartup failed";
        return false;
    }

    udp_socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_socket_ == INVALID_SOCKET) {
        LOG_ERROR << "Failed to create UDP socket: " << WSAGetLastError();
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.listen_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(udp_socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        LOG_ERROR << "Failed to bind UDP socket to port " << config_.listen_port
                  << " (Error: " << WSAGetLastError() << ")";
        closesocket(udp_socket_);
        udp_socket_ = INVALID_SOCKET;
        return false;
    }
    LOG_INFO << "Listening for WireGuard UDP traffic on 0.0.0.0:" << config_.listen_port;
#endif

    // Setup Wintun adapter if elevated
    try {
        if (NatForwarder::IsElevated()) {
            if (tun_adapter_.OpenOrCreate(L"openroute0", L"OpenRoute")) {
                tun_adapter_.StartSession();
                nat_forwarder_.SetupNat("OpenRouteNat", config_.tunnel_subnet);
                nat_forwarder_.EnableForwarding(config_.egress_interface, "openroute0");
            }
        } else {
            LOG_WARN << "Running in non-elevated mode. TUN adapter creation and NetNat deferred.";
        }
    } catch (const std::exception& ex) {
        LOG_WARN << "Network adapter setup deferred: " << ex.what();
    } catch (...) {
        LOG_WARN << "Network adapter setup deferred due to unknown error.";
    }

    is_running_ = true;

    udp_thread_ = std::thread(&GatewayServer::UdpReceiveLoop, this);
    tun_thread_ = std::thread(&GatewayServer::TunReceiveLoop, this);
    housekeeping_thread_ = std::thread(&GatewayServer::HousekeepingLoop, this);

    LOG_INFO << "OpenRoute Gateway Server started successfully.";
    return true;
}

void GatewayServer::Stop() {
    if (!is_running_) return;

    LOG_INFO << "Stopping OpenRoute Gateway Server...";
    is_running_ = false;

#if defined(_WIN32)
    if (udp_socket_ != INVALID_SOCKET) {
        closesocket(udp_socket_);
        udp_socket_ = INVALID_SOCKET;
    }
#endif

    tun_adapter_.Close();

    if (udp_thread_.joinable()) udp_thread_.join();
    if (tun_thread_.joinable()) tun_thread_.join();
    if (housekeeping_thread_.joinable()) housekeeping_thread_.join();

    quota_mgr_.PersistToDatabase();

#if defined(_WIN32)
    WSACleanup();
#endif

    LOG_INFO << "OpenRoute Gateway Server stopped.";
}

bool GatewayServer::ProcessInboundUdp(const uint8_t* data, size_t len,
                                      const std::string& remote_endpoint,
                                      std::vector<uint8_t>& out_udp_response,
                                      std::vector<uint8_t>& out_tun_packet) {
    out_udp_response.clear();
    out_tun_packet.clear();

    auto pkt_type = tunnel::GetPacketType(data, len);

    // 1. Handshake Initiation (Type 1)
    if (pkt_type == tunnel::PacketType::HandshakeInitiation && len == tunnel::kInitiationPacketSize) {
        const auto* init_pkt = reinterpret_cast<const tunnel::PacketHandshakeInitiation*>(data);
        security::KeyBuffer client_pub{};
        tunnel::HandshakeState hs_state{};

        if (!tunnel::NoiseProtocol::ConsumeInitiation(keypair_, *init_pkt, client_pub, hs_state)) {
            LOG_WARN << "Handshake Initiation authentication failed from " << remote_endpoint;
            return false;
        }

        std::string client_pub_b64 = security::Crypto::KeyToBase64(client_pub);
        LOG_INFO << "Handshake Initiation received from client: " << client_pub_b64;

        // Check if device is registered and APPROVED
        auto dev = device_mgr_.GetDeviceByPublicKey(client_pub_b64);
        if (!dev.has_value()) {
            LOG_WARN << "Unregistered device attempted connection: " << client_pub_b64;
            return false;
        }

        if (dev->status != device::DeviceStatus::Approved) {
            LOG_WARN << "Device connection rejected (Status: " << device::DeviceStatusToString(dev->status) << ")";
            return false;
        }

        // Ensure newly approved device is enrolled in quota manager & shaper
        accounting::PeerAccountingState act{};
        act.device_id = dev->id;
        act.expires_at = dev->expires_at;
        quota_mgr_.RegisterPeer(act);

        {
            std::lock_guard<std::mutex> lock(peer_mutex_);
            if (shapers_.find(dev->id) == shapers_.end()) {
                shapers_[dev->id] = std::make_unique<bandwidth::PeerBandwidthShaper>();
            }
        }

        // Quota / Expiry check
        if (quota_mgr_.CheckStatus(dev->id) == accounting::QuotaStatus::ExpiredPass ||
            quota_mgr_.CheckStatus(dev->id) == accounting::QuotaStatus::ExceededQuota) {
            LOG_WARN << "Device " << dev->id << " denied access due to exhausted quota or expired pass.";
            return false;
        }

        // Create Handshake Response (Type 2)
        tunnel::PacketHandshakeResponse resp_pkt{};
        tunnel::TransportKeys keys{};
        uint32_t my_index = static_cast<uint32_t>(rand());

        if (!tunnel::NoiseProtocol::CreateResponse(keypair_, {}, my_index, hs_state, resp_pkt, keys)) {
            LOG_ERROR << "Failed to create Handshake Response for " << dev->id;
            return false;
        }

        // Enroll active session
        std::lock_guard<std::mutex> lock(peer_mutex_);
        auto session = std::make_shared<tunnel::PeerSession>(dev->id, client_pub, dev->assigned_ip);
        session->CompleteHandshake(keys);
        session->SetRemoteEndpoint(remote_endpoint);

        sessions_by_index_[keys.local_index] = session;
        sessions_by_pubkey_[client_pub_b64] = session;
        sessions_by_ip_[dev->assigned_ip] = session;

        out_udp_response.assign(reinterpret_cast<const uint8_t*>(&resp_pkt),
                                reinterpret_cast<const uint8_t*>(&resp_pkt) + sizeof(resp_pkt));
        LOG_INFO << "Session established with peer: " << dev->id << " (assigned IP: " << dev->assigned_ip << ")";
        return true;
    }

    // 2. Transport Data (Type 4)
    if (pkt_type == tunnel::PacketType::TransportData && len >= tunnel::kMinTransportPacket) {
        const auto* hdr = reinterpret_cast<const tunnel::PacketTransportHeader*>(data);
        std::shared_ptr<tunnel::PeerSession> session;

        {
            std::lock_guard<std::mutex> lock(peer_mutex_);
            auto it = sessions_by_index_.find(hdr->receiver_index);
            if (it != sessions_by_index_.end()) {
                session = it->second;
            }
        }

        if (!session) {
            return false;
        }

        // Check Quota
        if (quota_mgr_.CheckStatus(session->peer_id()) != accounting::QuotaStatus::WithinQuota &&
            quota_mgr_.CheckStatus(session->peer_id()) != accounting::QuotaStatus::WarningApproaching) {
            return false;
        }

        // Decrypt
        std::vector<uint8_t> plaintext;
        if (!session->Decrypt(data, len, plaintext)) {
            return false;
        }

        // Check Bandwidth rate limiter (Ingress)
        {
            std::lock_guard<std::mutex> lock(peer_mutex_);
            auto shaper_it = shapers_.find(session->peer_id());
            if (shaper_it != shapers_.end()) {
                if (!shaper_it->second->CheckIngress(plaintext.size())) {
                    LOG_WARN << "Throttling ingress packet from peer " << session->peer_id();
                    return false; // dropped by token-bucket limiter
                }
            }
        }

        // Apply LAN Isolation Filter
        auto verdict = lan_filter_.InspectOutbound(plaintext.data(), plaintext.size());
        if (verdict != FilterVerdict::Allow) {
            LOG_WARN << "LAN Isolation Filter dropped packet from " << session->peer_id()
                     << " Verdict: " << VerdictToString(verdict);
            return false;
        }

        quota_mgr_.RecordTraffic(session->peer_id(), 0, plaintext.size());
        diag_engine_.UpdateThroughput(session->peer_id(), 0, plaintext.size(), 0.0);

        out_tun_packet = std::move(plaintext);
        return true;
    }

    // 3. Mobile IP-in-UDP Packet (Direct Guest Pass Tunnel)
    if (len >= 20 && (data[0] >> 4) == 4) {
        uint32_t src_ip = (static_cast<uint32_t>(data[12]) << 24) |
                          (static_cast<uint32_t>(data[13]) << 16) |
                          (static_cast<uint32_t>(data[14]) << 8) |
                          static_cast<uint32_t>(data[15]);
        std::string src_ip_str = LanIsolationFilter::IpToString(src_ip);

        // Track and enroll mobile session endpoint
        std::shared_ptr<tunnel::PeerSession> session;
        {
            std::lock_guard<std::mutex> lock(peer_mutex_);
            auto it = sessions_by_ip_.find(src_ip_str);
            if (it != sessions_by_ip_.end()) {
                session = it->second;
                session->SetRemoteEndpoint(remote_endpoint);
            } else {
                session = std::make_shared<tunnel::PeerSession>("peer_" + src_ip_str, security::KeyBuffer{}, src_ip_str);
                session->SetRemoteEndpoint(remote_endpoint);
                sessions_by_ip_[src_ip_str] = session;
            }
        }

        // Apply LAN Isolation Filter
        auto verdict = lan_filter_.InspectOutbound(data, len);
        if (verdict != FilterVerdict::Allow) {
            LOG_WARN << "LAN Isolation Filter dropped packet from " << src_ip_str
                     << " Verdict: " << VerdictToString(verdict);
            return false;
        }

        if (session) {
            quota_mgr_.RecordTraffic(session->peer_id(), 0, len);
            diag_engine_.UpdateThroughput(session->peer_id(), 0, len, 0.0);
        }

        out_tun_packet.assign(data, data + len);
        return true;
    }

    return false;
}

bool GatewayServer::ProcessInboundTun(const uint8_t* data, size_t len,
                                      std::vector<uint8_t>& out_udp_packet,
                                      std::string& out_remote_endpoint) {
    out_udp_packet.clear();
    out_remote_endpoint.clear();

    if (len < 20) return false;

    uint32_t dest_ip = ExtractDestIp(data, len);
    std::string dest_ip_str = LanIsolationFilter::IpToString(dest_ip);

    std::shared_ptr<tunnel::PeerSession> session;
    {
        std::lock_guard<std::mutex> lock(peer_mutex_);
        auto it = sessions_by_ip_.find(dest_ip_str);
        if (it != sessions_by_ip_.end()) {
            session = it->second;
        }
    }

    if (!session || session->remote_endpoint().empty()) {
        return false;
    }

    // Check Bandwidth rate limiter (Egress)
    {
        std::lock_guard<std::mutex> lock(peer_mutex_);
        auto shaper_it = shapers_.find(session->peer_id());
        if (shaper_it != shapers_.end()) {
            if (!shaper_it->second->CheckEgress(len)) {
                LOG_WARN << "Throttling egress packet to peer " << session->peer_id();
                return false;
            }
        }
    }

    // Encrypt packet to peer if full WireGuard session, else send raw IP
    if (session->state() == tunnel::SessionState::Established) {
        if (!session->Encrypt(data, len, out_udp_packet)) {
            return false;
        }
    } else {
        out_udp_packet.assign(data, data + len);
    }

    out_remote_endpoint = session->remote_endpoint();
    quota_mgr_.RecordTraffic(session->peer_id(), len, 0);
    diag_engine_.UpdateThroughput(session->peer_id(), len, 0, 0.0);
    return true;
}

void GatewayServer::UdpReceiveLoop() {
#if defined(_WIN32)
    std::vector<uint8_t> buffer(65536);
    sockaddr_in sender_addr{};
    int sender_len = sizeof(sender_addr);

    while (is_running_) {
        try {
            int bytes = recvfrom(udp_socket_, reinterpret_cast<char*>(buffer.data()),
                                 static_cast<int>(buffer.size()), 0,
                                 reinterpret_cast<sockaddr*>(&sender_addr), &sender_len);
            if (bytes <= 0) {
                if (!is_running_) break;
                continue;
            }

            char ip_str[INET_ADDRSTRLEN] = {0};
            inet_ntop(AF_INET, &(sender_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
            std::string endpoint = std::string(ip_str) + ":" + std::to_string(ntohs(sender_addr.sin_port));

            std::vector<uint8_t> udp_response;
            std::vector<uint8_t> tun_packet;

            if (ProcessInboundUdp(buffer.data(), static_cast<size_t>(bytes), endpoint, udp_response, tun_packet)) {
                if (!udp_response.empty()) {
                    sendto(udp_socket_, reinterpret_cast<const char*>(udp_response.data()),
                           static_cast<int>(udp_response.size()), 0,
                           reinterpret_cast<sockaddr*>(&sender_addr), sender_len);
                }
                if (!tun_packet.empty()) {
                    if (tun_adapter_.IsActive()) {
                        tun_adapter_.WritePacket(tun_packet.data(), tun_packet.size());
                    } else {
                        // User-space DNS forwarder fallback when Wintun is not active:
                        // If it's a UDP packet to port 53 (DNS), forward directly to upstream DNS (8.8.8.8)
                        // and return response to client!
                        if (tun_packet.size() >= 28 && tun_packet[9] == 17) { // IPv4 + UDP
                            uint16_t dest_port = (static_cast<uint16_t>(tun_packet[22]) << 8) | tun_packet[23];
                            if (dest_port == 53) {
                                size_t dns_len = tun_packet.size() - 28;
                                sockaddr_in upstream_dns{};
                                upstream_dns.sin_family = AF_INET;
                                upstream_dns.sin_port = htons(53);
                                inet_pton(AF_INET, "8.8.8.8", &upstream_dns.sin_addr);

                                SOCKET forward_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                                if (forward_sock != INVALID_SOCKET) {
                                    DWORD timeout = 2000;
                                    setsockopt(forward_sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
                                    sendto(forward_sock, reinterpret_cast<const char*>(tun_packet.data() + 28),
                                           static_cast<int>(dns_len), 0,
                                           reinterpret_cast<sockaddr*>(&upstream_dns), sizeof(upstream_dns));

                                    std::vector<uint8_t> dns_resp_buf(4096);
                                    int resp_len = recv(forward_sock, reinterpret_cast<char*>(dns_resp_buf.data()), static_cast<int>(dns_resp_buf.size()), 0);
                                    closesocket(forward_sock);

                                    if (resp_len > 0) {
                                        std::vector<uint8_t> reply_ip_pkt(28 + resp_len);
                                        // IPv4 header (20 bytes)
                                        reply_ip_pkt[0] = 0x45; // Version 4, IHL 5
                                        reply_ip_pkt[1] = 0x00; // DSCP / ECN
                                        uint16_t total_len = static_cast<uint16_t>(28 + resp_len);
                                        reply_ip_pkt[2] = static_cast<uint8_t>(total_len >> 8);
                                        reply_ip_pkt[3] = static_cast<uint8_t>(total_len & 0xFF);
                                        reply_ip_pkt[8] = 64;   // TTL
                                        reply_ip_pkt[9] = 17;   // UDP
                                        // Source IP: 8.8.8.8
                                        reply_ip_pkt[12] = 8; reply_ip_pkt[13] = 8; reply_ip_pkt[14] = 8; reply_ip_pkt[15] = 8;
                                        // Dest IP: client IP from incoming packet (tun_packet[12..15])
                                        reply_ip_pkt[16] = tun_packet[12]; reply_ip_pkt[17] = tun_packet[13];
                                        reply_ip_pkt[18] = tun_packet[14]; reply_ip_pkt[19] = tun_packet[15];
                                        // UDP header (8 bytes)
                                        reply_ip_pkt[20] = 0x00; reply_ip_pkt[21] = 53; // Source Port: 53
                                        reply_ip_pkt[22] = tun_packet[20]; reply_ip_pkt[23] = tun_packet[21]; // Dest Port: client src port
                                        uint16_t udp_len = static_cast<uint16_t>(8 + resp_len);
                                        reply_ip_pkt[24] = static_cast<uint8_t>(udp_len >> 8);
                                        reply_ip_pkt[25] = static_cast<uint8_t>(udp_len & 0xFF);
                                        // Copy DNS payload
                                        std::memcpy(reply_ip_pkt.data() + 28, dns_resp_buf.data(), resp_len);

                                        sendto(udp_socket_, reinterpret_cast<const char*>(reply_ip_pkt.data()),
                                               static_cast<int>(reply_ip_pkt.size()), 0,
                                               reinterpret_cast<sockaddr*>(&sender_addr), sender_len);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } catch (const std::exception& ex) {
            LOG_ERROR << "[GatewayServer] UdpReceiveLoop error: " << ex.what();
        } catch (...) {
            LOG_ERROR << "[GatewayServer] UdpReceiveLoop unknown error";
        }
    }
#endif
}

void GatewayServer::TunReceiveLoop() {
    std::vector<uint8_t> packet;
    while (is_running_) {
        try {
            if (!tun_adapter_.IsActive()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            if (tun_adapter_.ReadPacket(packet)) {
                std::vector<uint8_t> udp_pkt;
                std::string endpoint;
                if (ProcessInboundTun(packet.data(), packet.size(), udp_pkt, endpoint)) {
#if defined(_WIN32)
                    if (!endpoint.empty() && udp_socket_ != INVALID_SOCKET) {
                        auto pos = endpoint.find(':');
                        if (pos != std::string::npos) {
                            std::string ip = endpoint.substr(0, pos);
                            uint16_t port = static_cast<uint16_t>(std::stoi(endpoint.substr(pos + 1)));

                            sockaddr_in dest{};
                            dest.sin_family = AF_INET;
                            dest.sin_port = htons(port);
                            inet_pton(AF_INET, ip.c_str(), &(dest.sin_addr));

                            sendto(udp_socket_, reinterpret_cast<const char*>(udp_pkt.data()),
                                   static_cast<int>(udp_pkt.size()), 0,
                                   reinterpret_cast<sockaddr*>(&dest), sizeof(dest));
                        }
                    }
#endif
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        } catch (const std::exception& ex) {
            LOG_ERROR << "[GatewayServer] TunReceiveLoop error: " << ex.what();
        } catch (...) {
            LOG_ERROR << "[GatewayServer] TunReceiveLoop unknown error";
        }
    }
}

void GatewayServer::HousekeepingLoop() {
    while (is_running_) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        if (!is_running_) break;

        try {
            quota_mgr_.PersistToDatabase();
        } catch (const std::exception& ex) {
            LOG_ERROR << "[GatewayServer] Quota persistence error: " << ex.what();
        } catch (...) {
            LOG_ERROR << "[GatewayServer] Housekeeping unknown error";
        }
    }
}

} // namespace openroute::gateway
