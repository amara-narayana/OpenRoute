#pragma once

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "core/storage/database.hpp"
#include "core/security/crypto.hpp"
#include "core/tunnel/peer_session.hpp"
#include "core/tunnel/wintun_adapter.hpp"
#include "core/gateway/nat_forwarder.hpp"
#include "core/gateway/lan_isolation.hpp"
#include "core/bandwidth/token_bucket.hpp"
#include "core/accounting/quota_manager.hpp"
#include "core/accounting/billing_manager.hpp"
#include "core/device/device_manager.hpp"
#include "core/policy/invitation_manager.hpp"
#include "core/diagnostics/diagnostics_engine.hpp"
#include "core/gateway/universal_proxy.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace openroute::gateway {

struct GatewayConfig {
    std::string id = "gw_default";
    std::string name = "OpenRoute Gateway";
    uint16_t listen_port = 51820;
    std::string tunnel_subnet = "10.77.0.0/24";
    std::string tunnel_ip = "10.77.0.1";
    std::string egress_interface = "Wi-Fi";
};

class GatewayServer {
public:
    GatewayServer(storage::Database& db, GatewayConfig config = {});
    ~GatewayServer();

    bool Initialize();
    bool Start();
    void Stop();

    [[nodiscard]] bool IsRunning() const { return is_running_; }
    [[nodiscard]] const GatewayConfig& config() const { return config_; }
    [[nodiscard]] const security::KeyPair& keypair() const { return keypair_; }

    device::DeviceManager& device_manager() { return device_mgr_; }
    policy::InvitationManager& invitation_manager() { return invitation_mgr_; }
    accounting::QuotaManager& quota_manager() { return quota_mgr_; }
    accounting::BillingManager& billing_manager() { return billing_mgr_; }
    diagnostics::DiagnosticsEngine& diagnostics_engine() { return diag_engine_; }
    UniversalProxy* proxy() { return proxy_.get(); }
    storage::Database& database() { return db_; }

    // Direct programmatic packet injection for integration testing and loopback verification
    bool ProcessInboundUdp(const uint8_t* data, size_t len,
                          const std::string& remote_endpoint,
                          std::vector<uint8_t>& out_udp_response,
                          std::vector<uint8_t>& out_tun_packet);

    bool ProcessInboundTun(const uint8_t* data, size_t len,
                          std::vector<uint8_t>& out_udp_packet,
                          std::string& out_remote_endpoint);

private:
    void UdpReceiveLoop();
    void TunReceiveLoop();
    void HousekeepingLoop();

    GatewayConfig config_;
    storage::Database& db_;
    security::KeyPair keypair_{};

    device::DeviceManager device_mgr_;
    policy::InvitationManager invitation_mgr_;
    accounting::QuotaManager quota_mgr_;
    accounting::BillingManager billing_mgr_;
    diagnostics::DiagnosticsEngine diag_engine_;
    LanIsolationFilter lan_filter_;
    NatForwarder nat_forwarder_;
    tunnel::WintunAdapter tun_adapter_;
    std::unique_ptr<UniversalProxy> proxy_;

    std::atomic<bool> is_running_{false};
    std::thread udp_thread_;
    std::thread tun_thread_;
    std::thread housekeeping_thread_;

#if defined(_WIN32)
    SOCKET udp_socket_ = INVALID_SOCKET;
#endif

    mutable std::mutex peer_mutex_;
    // remote_index -> PeerSession
    std::unordered_map<uint32_t, std::shared_ptr<tunnel::PeerSession>> sessions_by_index_;
    // public_key (b64) -> PeerSession
    std::unordered_map<std::string, std::shared_ptr<tunnel::PeerSession>> sessions_by_pubkey_;
    // assigned_ip -> PeerSession
    std::unordered_map<std::string, std::shared_ptr<tunnel::PeerSession>> sessions_by_ip_;
    // device_id -> PeerBandwidthShaper
    std::unordered_map<std::string, std::unique_ptr<bandwidth::PeerBandwidthShaper>> shapers_;
};

} // namespace openroute::gateway
