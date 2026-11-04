#include "tests/test_framework.hpp"
#include "core/tunnel/client_engine.hpp"
#include "core/gateway/gateway_server.hpp"
#include "core/storage/database.hpp"

using namespace openroute::tunnel;
using namespace openroute::gateway;
using namespace openroute::security;

namespace {
// Helper to construct a minimal simulated IPv4 header
std::vector<uint8_t> CreateSimulatedIpv4Packet(uint32_t src_ip, uint32_t dst_ip, const std::string& payload) {
    std::vector<uint8_t> pkt(20 + payload.size(), 0);
    pkt[0] = 0x45; // Version 4, IHL 5 (20 bytes)
    pkt[8] = 64;   // TTL
    pkt[9] = 17;   // Protocol UDP

    pkt[12] = static_cast<uint8_t>((src_ip >> 24) & 0xFF);
    pkt[13] = static_cast<uint8_t>((src_ip >> 16) & 0xFF);
    pkt[14] = static_cast<uint8_t>((src_ip >> 8) & 0xFF);
    pkt[15] = static_cast<uint8_t>(src_ip & 0xFF);

    pkt[16] = static_cast<uint8_t>((dst_ip >> 24) & 0xFF);
    pkt[17] = static_cast<uint8_t>((dst_ip >> 16) & 0xFF);
    pkt[18] = static_cast<uint8_t>((dst_ip >> 8) & 0xFF);
    pkt[19] = static_cast<uint8_t>(dst_ip & 0xFF);

    std::memcpy(pkt.data() + 20, payload.data(), payload.size());
    return pkt;
}
} // namespace

TEST(ClientEngineTest, InvitationUrlParsing) {
    ClientEngine client;
    GatewayProfile profile;
    std::string token;

    std::string url = "openroute://invite?token=SEC123TOKEN&gw=gw_home&name=HomeOffice&pub=dGhpcyBpcyBhbiBleGFtcGxlIHB1YmxpYyBrZXk=&port=51820&host=192.168.1.100";

    EXPECT_TRUE(client.ParseInvitationUrl(url, profile, token));
    EXPECT_EQ(token, "SEC123TOKEN");
    EXPECT_EQ(profile.gateway_id, "gw_home");
    EXPECT_EQ(profile.gateway_name, "HomeOffice");
    EXPECT_EQ(profile.gateway_public_key_b64, "dGhpcyBpcyBhbiBleGFtcGxlIHB1YmxpYyBrZXk=");
    EXPECT_EQ(profile.endpoint_port, 51820);
    EXPECT_EQ(profile.endpoint_host, "192.168.1.100");

    // Invalid scheme
    GatewayProfile bad_prof;
    std::string bad_tok;
    EXPECT_FALSE(client.ParseInvitationUrl("https://invalid.com/invite", bad_prof, bad_tok));
}

TEST(ClientEngineTest, ClientHandshakeAndTransportExchange) {
    openroute::storage::Database db;
    EXPECT_TRUE(db.Open(":memory:"));
    EXPECT_TRUE(db.ApplyMigrations());

    GatewayConfig cfg{};
    cfg.id = "gw_client_test";
    cfg.name = "Test Gateway";
    GatewayServer server(db, cfg);
    EXPECT_TRUE(server.Initialize());

    // Setup Client
    auto client_kp = Crypto::GenerateKeyPair();
    std::string client_pub_b64 = Crypto::KeyToBase64(client_kp.public_key);
    std::string client_priv_b64 = Crypto::KeyToBase64(client_kp.private_key);

    ClientEngine client;
    GatewayProfile prof{};
    prof.gateway_id = cfg.id;
    prof.gateway_name = cfg.name;
    prof.endpoint_host = "127.0.0.1";
    prof.endpoint_port = 51820;
    prof.gateway_public_key_b64 = Crypto::KeyToBase64(server.keypair().public_key);
    prof.client_public_key_b64 = client_pub_b64;
    prof.client_private_key_b64 = client_priv_b64;
    prof.enable_kill_switch = true;

    EXPECT_TRUE(client.Configure(prof));

    // Register & Approve client on server
    std::string dev_id, assigned_ip;
    EXPECT_TRUE(server.device_manager().RegisterDeviceRequest(
        cfg.id, "Test Laptop", "windows", client_pub_b64,
        2000000000, dev_id, assigned_ip));
    EXPECT_TRUE(server.device_manager().ApproveDevice(dev_id));

    // Simulate Handshake between Client and Server
    PacketHandshakeInitiation init_pkt{};
    HandshakeState client_hs{};

    // Client creates Initiation (Type 1)
    auto gw_pub = Crypto::KeyFromBase64(prof.gateway_public_key_b64);
    EXPECT_TRUE(gw_pub.has_value());

    EXPECT_TRUE(NoiseProtocol::CreateInitiation(
        client_kp, *gw_pub, {}, 101, init_pkt, client_hs));

    // Server processes Initiation and generates Response (Type 2)
    std::vector<uint8_t> udp_response;
    std::vector<uint8_t> tun_packet;
    EXPECT_TRUE(server.ProcessInboundUdp(
        reinterpret_cast<const uint8_t*>(&init_pkt), sizeof(init_pkt),
        "127.0.0.1:54321", udp_response, tun_packet));

    EXPECT_EQ(udp_response.size(), sizeof(PacketHandshakeResponse));

    // Client processes Response
    const auto* resp_pkt = reinterpret_cast<const PacketHandshakeResponse*>(udp_response.data());
    TransportKeys client_keys{};
    EXPECT_TRUE(NoiseProtocol::ConsumeResponse(
        client_kp, {}, *resp_pkt, client_hs, client_keys));

    // Client Encrypts simulated IPv4 packet towards Google DNS (8.8.8.8)
    const std::string payload = "Simulated DNS packet to 8.8.8.8";
    auto ip_pkt = CreateSimulatedIpv4Packet(0x0A4D0002, 0x08080808, payload);
    std::vector<uint8_t> transport_wire;
    EXPECT_TRUE(NoiseProtocol::EncryptTransport(
        client_keys, 0, ip_pkt.data(), ip_pkt.size(), transport_wire));

    // Server decrypts packet
    std::vector<uint8_t> gw_out_tun;
    std::vector<uint8_t> gw_udp_resp2;
    EXPECT_TRUE(server.ProcessInboundUdp(
        transport_wire.data(), transport_wire.size(),
        "127.0.0.1:54321", gw_udp_resp2, gw_out_tun));

    EXPECT_EQ(gw_out_tun.size(), ip_pkt.size());
    EXPECT_TRUE(Crypto::ConstantTimeEqual(gw_out_tun.data(), ip_pkt.data(), ip_pkt.size()));
}

TEST(ClientEngineTest, FailClosedKillSwitchBehavior) {
    ClientEngine client;
    GatewayProfile prof{};
    prof.gateway_id = "gw_ks";
    prof.gateway_name = "KillSwitch GW";
    prof.gateway_public_key_b64 = Crypto::KeyToBase64(Crypto::GenerateRandomKey());
    prof.endpoint_host = "192.0.2.1"; // Unreachable dummy IP
    prof.endpoint_port = 51820;
    prof.enable_kill_switch = true;

    EXPECT_TRUE(client.Configure(prof));
    EXPECT_FALSE(client.is_kill_switch_active());

    // Attempt to connect to unreachable host -> fails
    bool connected = client.Connect();
    EXPECT_FALSE(connected);
    EXPECT_EQ(client.state(), ClientState::Failed);

    // Fail-closed kill switch must be ACTIVE to prevent traffic leak
    EXPECT_TRUE(client.is_kill_switch_active());

    // Explicit disconnect maintains fail-closed protection
    client.Disconnect();
    EXPECT_TRUE(client.is_kill_switch_active());
}
