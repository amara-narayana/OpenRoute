#include "tests/test_framework.hpp"
#include "core/storage/database.hpp"
#include "core/gateway/gateway_server.hpp"
#include "core/gateway/lan_isolation.hpp"
#include "core/bandwidth/token_bucket.hpp"
#include "core/accounting/quota_manager.hpp"
#include "core/device/device_manager.hpp"
#include "core/policy/invitation_manager.hpp"

#include <chrono>
#include <thread>

using namespace openroute::storage;
using namespace openroute::gateway;
using namespace openroute::bandwidth;
using namespace openroute::accounting;
using namespace openroute::device;
using namespace openroute::policy;
using namespace openroute::security;

namespace {
// Helper to construct a minimal simulated IPv4 header
std::vector<uint8_t> CreateSimulatedIpv4Packet(uint32_t src_ip, uint32_t dst_ip, const std::string& payload) {
    std::vector<uint8_t> pkt(20 + payload.size(), 0);
    pkt[0] = 0x45; // Version 4, IHL 5 (20 bytes)
    pkt[8] = 64;   // TTL
    pkt[9] = 17;   // Protocol UDP

    // Source IP
    pkt[12] = static_cast<uint8_t>((src_ip >> 24) & 0xFF);
    pkt[13] = static_cast<uint8_t>((src_ip >> 16) & 0xFF);
    pkt[14] = static_cast<uint8_t>((src_ip >> 8) & 0xFF);
    pkt[15] = static_cast<uint8_t>(src_ip & 0xFF);

    // Dest IP
    pkt[16] = static_cast<uint8_t>((dst_ip >> 24) & 0xFF);
    pkt[17] = static_cast<uint8_t>((dst_ip >> 16) & 0xFF);
    pkt[18] = static_cast<uint8_t>((dst_ip >> 8) & 0xFF);
    pkt[19] = static_cast<uint8_t>(dst_ip & 0xFF);

    std::memcpy(pkt.data() + 20, payload.data(), payload.size());
    return pkt;
}
} // namespace

TEST(GatewayPipelineTest, LanIsolationFilterRules) {
    LanIsolationFilter filter;
    filter.SetTunnelGatewayIp(0x0A4D0001); // 10.77.0.1
    filter.SetAllowLanAccess(false);

    // 1. Packet targeting public internet (1.1.1.1) -> ALLOW
    auto pkt_public = CreateSimulatedIpv4Packet(0x0A4D0002, 0x01010101, "DNS Query to Cloudflare");
    EXPECT_EQ(filter.InspectOutbound(pkt_public.data(), pkt_public.size()), FilterVerdict::Allow);

    // 2. Packet targeting Google DNS (8.8.8.8) -> ALLOW
    auto pkt_google = CreateSimulatedIpv4Packet(0x0A4D0002, 0x08080808, "DNS Query to Google");
    EXPECT_EQ(filter.InspectOutbound(pkt_google.data(), pkt_google.size()), FilterVerdict::Allow);

    // 3. Packet targeting gateway tunnel interface (10.77.0.1) -> ALLOW
    auto pkt_gw_tunnel = CreateSimulatedIpv4Packet(0x0A4D0002, 0x0A4D0001, "Ping Gateway Tunnel");
    EXPECT_EQ(filter.InspectOutbound(pkt_gw_tunnel.data(), pkt_gw_tunnel.size()), FilterVerdict::Allow);

    // 4. Packet targeting private home router (192.168.0.1) -> DROP_PRIVATE_LAN
    auto pkt_home_router = CreateSimulatedIpv4Packet(0x0A4D0002, 0xC0A80001, "Attempt access to Home Router");
    EXPECT_EQ(filter.InspectOutbound(pkt_home_router.data(), pkt_home_router.size()), FilterVerdict::DropPrivateLan);

    // 5. Packet targeting private corporate range (10.0.0.50) -> DROP_PRIVATE_LAN
    auto pkt_corp_lan = CreateSimulatedIpv4Packet(0x0A4D0002, 0x0A000032, "Attempt access to LAN NAS");
    EXPECT_EQ(filter.InspectOutbound(pkt_corp_lan.data(), pkt_corp_lan.size()), FilterVerdict::DropPrivateLan);

    // 6. Packet targeting link-local address (169.254.1.1) -> DROP_PRIVATE_LAN
    auto pkt_link_local = CreateSimulatedIpv4Packet(0x0A4D0002, 0xA9FE0101, "Attempt link-local access");
    EXPECT_EQ(filter.InspectOutbound(pkt_link_local.data(), pkt_link_local.size()), FilterVerdict::DropPrivateLan);

    // 7. Packet targeting loopback (127.0.0.1) -> DROP_LOOPBACK
    auto pkt_loopback = CreateSimulatedIpv4Packet(0x0A4D0002, 0x7F000001, "Attempt loopback access");
    EXPECT_EQ(filter.InspectOutbound(pkt_loopback.data(), pkt_loopback.size()), FilterVerdict::DropLoopback);
}

TEST(GatewayPipelineTest, BandwidthTokenBucketLimiting) {
    // 50,000 bytes/sec rate, 10,000 bytes burst capacity
    TokenBucket bucket(50000, 10000);

    // Initial burst up to 10,000 bytes should succeed
    EXPECT_TRUE(bucket.Consume(5000));
    EXPECT_TRUE(bucket.Consume(4000));

    // Consuming more than remaining tokens without waiting should fail
    EXPECT_FALSE(bucket.Consume(5000));

    // Wait 100ms: should replenish ~5,000 tokens (50,000 * 0.1s)
    std::this_thread::sleep_for(std::chrono::milliseconds(110));
    EXPECT_TRUE(bucket.Consume(4000));
}

TEST(GatewayPipelineTest, QuotaEnforcementAndWarnings) {
    QuotaManager qm;

    PeerAccountingState peer{};
    peer.device_id = "test_dev_01";
    peer.daily_quota_bytes = 10000; // 10 KB daily quota
    qm.RegisterPeer(peer);

    // Initial check: within quota
    EXPECT_EQ(qm.CheckStatus("test_dev_01"), QuotaStatus::WithinQuota);

    // Send 5,000 bytes -> within quota
    qm.RecordTraffic("test_dev_01", 3000, 2000);
    EXPECT_EQ(qm.CheckStatus("test_dev_01"), QuotaStatus::WithinQuota);

    // Send 3,500 bytes -> total 8,500 bytes (85% of 10,000) -> WarningApproaching (> 80%)
    qm.RecordTraffic("test_dev_01", 2000, 1500);
    EXPECT_EQ(qm.CheckStatus("test_dev_01"), QuotaStatus::WarningApproaching);

    // Send 2,000 bytes -> total 10,500 bytes (> 10,000) -> ExceededQuota
    qm.RecordTraffic("test_dev_01", 1000, 1000);
    EXPECT_EQ(qm.CheckStatus("test_dev_01"), QuotaStatus::ExceededQuota);

    // Reset quota
    qm.ResetUsage("test_dev_01");
    EXPECT_EQ(qm.CheckStatus("test_dev_01"), QuotaStatus::WithinQuota);
}

TEST(GatewayPipelineTest, InvitationAndApprovalLifecycle) {
    openroute::storage::Database db;
    EXPECT_TRUE(db.Open(":memory:"));
    EXPECT_TRUE(db.ApplyMigrations());

    GatewayConfig cfg{};
    cfg.id = "gw_lifecycle";
    cfg.name = "Test Gateway";
    GatewayServer server(db, cfg);
    EXPECT_TRUE(server.Initialize());

    auto gw_pub_b64 = Crypto::KeyToBase64(server.keypair().public_key);

    // 1. Gateway owner creates a 24-hour invitation pass
    auto invite = server.invitation_manager().CreateInvitation(
        cfg.id, cfg.name, gw_pub_b64, 51820, "android", "TEMPORARY_24H", 86400, 900, 1);

    EXPECT_TRUE(invite.has_value());
    EXPECT_FALSE(invite->token.empty());
    EXPECT_TRUE(invite->invitation_url.find("openroute://invite") != std::string::npos);

    // 2. Client redeems the invitation token
    int64_t pass_duration = 0;
    std::string redeemed_gw_id;
    EXPECT_TRUE(server.invitation_manager().ValidateAndRedeemToken(
        invite->token, pass_duration, redeemed_gw_id));
    EXPECT_EQ(redeemed_gw_id, cfg.id);
    EXPECT_EQ(pass_duration, 86400);

    // Redeeming the single-use token a second time must fail
    EXPECT_FALSE(server.invitation_manager().ValidateAndRedeemToken(
        invite->token, pass_duration, redeemed_gw_id));

    // 3. Client registers as PENDING_APPROVAL
    auto now_epoch = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    auto client_keypair = Crypto::GenerateKeyPair();
    std::string client_pub_b64 = Crypto::KeyToBase64(client_keypair.public_key);
    std::string device_id, assigned_ip;

    EXPECT_TRUE(server.device_manager().RegisterDeviceRequest(
        cfg.id, "Alice's Android Phone", "android", client_pub_b64,
        now_epoch + pass_duration, device_id, assigned_ip));

    EXPECT_EQ(assigned_ip, "10.77.0.2");

    // Device is in pending state
    auto pending = server.device_manager().ListPendingRequests(cfg.id);
    EXPECT_EQ(pending.size(), 1U);
    EXPECT_EQ(pending[0].id, device_id);

    // 4. Client attempts Handshake while still PENDING_APPROVAL -> Gateway must reject!
    openroute::tunnel::PacketHandshakeInitiation init_pkt{};
    openroute::tunnel::HandshakeState client_hs{};
    openroute::tunnel::NoiseProtocol::CreateInitiation(
        client_keypair, server.keypair().public_key, {}, 101, init_pkt, client_hs);

    std::vector<uint8_t> udp_resp;
    std::vector<uint8_t> tun_pkt;
    bool admitted = server.ProcessInboundUdp(
        reinterpret_cast<const uint8_t*>(&init_pkt), sizeof(init_pkt),
        "192.168.0.50:51820", udp_resp, tun_pkt);

    EXPECT_FALSE(admitted); // Rejected! Device is not approved yet.

    // 5. Gateway owner approves device
    EXPECT_TRUE(server.device_manager().ApproveDevice(device_id));
    EXPECT_EQ(server.device_manager().ListPendingRequests(cfg.id).size(), 0U);

    // 6. Client attempts Handshake now -> Gateway accepts and replies with Type 2 Response!
    openroute::tunnel::PacketHandshakeInitiation init_pkt2{};
    openroute::tunnel::HandshakeState client_hs2{};
    openroute::tunnel::NoiseProtocol::CreateInitiation(
        client_keypair, server.keypair().public_key, {}, 102, init_pkt2, client_hs2);

    admitted = server.ProcessInboundUdp(
        reinterpret_cast<const uint8_t*>(&init_pkt2), sizeof(init_pkt2),
        "192.168.0.50:51820", udp_resp, tun_pkt);

    EXPECT_TRUE(admitted);
    EXPECT_EQ(udp_resp.size(), sizeof(openroute::tunnel::PacketHandshakeResponse));

    // 7. Client consumes response and establishes session
    openroute::tunnel::TransportKeys client_keys{};
    const auto* resp_pkt = reinterpret_cast<const openroute::tunnel::PacketHandshakeResponse*>(udp_resp.data());
    EXPECT_TRUE(openroute::tunnel::NoiseProtocol::ConsumeResponse(
        client_keypair, {}, *resp_pkt, client_hs2, client_keys));

    // 8. Client sends transport packet heading to public internet (1.1.1.1)
    auto wan_pkt = CreateSimulatedIpv4Packet(0x0A4D0002, 0x01010101, "GET / HTTP/1.1");
    std::vector<uint8_t> wire_transport;
    EXPECT_TRUE(openroute::tunnel::NoiseProtocol::EncryptTransport(
        client_keys, 0, wan_pkt.data(), wan_pkt.size(), wire_transport));

    std::vector<uint8_t> gw_out_tun;
    std::vector<uint8_t> gw_udp_resp2;
    EXPECT_TRUE(server.ProcessInboundUdp(
        wire_transport.data(), wire_transport.size(),
        "192.168.0.50:51820", gw_udp_resp2, gw_out_tun));

    // Gateway decrypted and forwarded to virtual TUN interface
    EXPECT_EQ(gw_out_tun.size(), wan_pkt.size());
    EXPECT_TRUE(Crypto::ConstantTimeEqual(gw_out_tun.data(), wan_pkt.data(), wan_pkt.size()));

    // 9. Now test Return Path (Internet response to client)
    auto return_wan_pkt = CreateSimulatedIpv4Packet(0x01010101, 0x0A4D0002, "HTTP/1.1 200 OK");
    std::vector<uint8_t> gw_to_client_udp;
    std::string client_ep;

    EXPECT_TRUE(server.ProcessInboundTun(
        return_wan_pkt.data(), return_wan_pkt.size(), gw_to_client_udp, client_ep));

    EXPECT_EQ(client_ep, "192.168.0.50:51820");

    // Client decrypts return packet
    uint64_t client_counter = 0;
    std::vector<uint8_t> client_decrypted_return;
    EXPECT_TRUE(openroute::tunnel::NoiseProtocol::DecryptTransport(
        client_keys, gw_to_client_udp.data(), gw_to_client_udp.size(),
        client_counter, client_decrypted_return));

    EXPECT_EQ(client_decrypted_return.size(), return_wan_pkt.size());
    EXPECT_TRUE(Crypto::ConstantTimeEqual(
        client_decrypted_return.data(), return_wan_pkt.data(), return_wan_pkt.size()));

    // 10. Gateway owner REVOKES device
    EXPECT_TRUE(server.device_manager().RevokeDevice(device_id));
    server.quota_manager().SetRevoked(device_id, true);

    // Client attempts to send another packet -> MUST BE REJECTED IMMEDIATELY
    std::vector<uint8_t> wire_transport2;
    EXPECT_TRUE(openroute::tunnel::NoiseProtocol::EncryptTransport(
        client_keys, 1, wan_pkt.data(), wan_pkt.size(), wire_transport2));

    EXPECT_FALSE(server.ProcessInboundUdp(
        wire_transport2.data(), wire_transport2.size(),
        "192.168.0.50:51820", gw_udp_resp2, gw_out_tun));
}
