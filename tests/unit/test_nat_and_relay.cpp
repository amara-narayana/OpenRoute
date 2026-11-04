#include "tests/test_framework.hpp"
#include "core/networking/nat_probe.hpp"
#include "core/relay/relay_client.hpp"
#include "core/security/crypto.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>

using namespace openroute::networking;
using namespace openroute::relay;
using namespace openroute::security;

TEST(NatProbeTest, BuildBindingRequestFormat) {
    std::array<uint8_t, 12> tx_id;
    for (size_t i = 0; i < 12; ++i) {
        tx_id[i] = static_cast<uint8_t>(0xA0 + i);
    }

    auto req = NatProbe::BuildBindingRequest(tx_id);
    ASSERT_EQ(req.size(), 20u);

    // Message type 0x0001
    EXPECT_EQ(req[0], 0x00);
    EXPECT_EQ(req[1], 0x01);

    // Length 0x0000
    EXPECT_EQ(req[2], 0x00);
    EXPECT_EQ(req[3], 0x00);

    // Magic cookie 0x2112A442
    EXPECT_EQ(req[4], 0x21);
    EXPECT_EQ(req[5], 0x12);
    EXPECT_EQ(req[6], 0xA4);
    EXPECT_EQ(req[7], 0x42);

    // Transaction ID
    EXPECT_EQ(std::memcmp(&req[8], tx_id.data(), 12), 0);
}

TEST(NatProbeTest, ParseXorMappedAddress) {
    std::array<uint8_t, 12> tx_id;
    Crypto::GenerateRandom(tx_id.data(), tx_id.size());

    // Construct a simulated STUN Binding Success Response (0x0101)
    // with XOR-MAPPED-ADDRESS: IPv4, Port 51820 (0xCA6C), IP 198.51.100.42 (0xC633642A)
    std::vector<uint8_t> resp;
    resp.resize(20 + 12); // 20-byte header + 4-byte attr header + 8-byte XOR attr value

    // Header
    resp[0] = 0x01; resp[1] = 0x01; // Binding Response
    resp[2] = 0x00; resp[3] = 0x0C; // Attribute length = 12 bytes
    resp[4] = 0x21; resp[5] = 0x12; resp[6] = 0xA4; resp[7] = 0x42; // Magic Cookie
    std::memcpy(&resp[8], tx_id.data(), 12);

    // Attribute Header: Type 0x0020 (XOR-MAPPED-ADDRESS), Length = 8
    resp[20] = 0x00; resp[21] = 0x20;
    resp[22] = 0x00; resp[23] = 0x08;

    // Value:
    // Byte 0: 0x00 reserved
    // Byte 1: 0x01 family IPv4
    resp[24] = 0x00;
    resp[25] = 0x01;

    // XOR Port: 51820 (0xCA6C) ^ (0x2112) = 0xEB7E
    uint16_t real_port = 51820;
    uint16_t xor_port = real_port ^ static_cast<uint16_t>(NatProbe::STUN_MAGIC_COOKIE >> 16);
    resp[26] = static_cast<uint8_t>(xor_port >> 8);
    resp[27] = static_cast<uint8_t>(xor_port & 0xFF);

    // XOR IP: 198.51.100.42 = 0xC633642A ^ 0x2112A442 = 0xE721C068
    uint32_t real_ip = (198u << 24) | (51u << 16) | (100u << 8) | 42u;
    uint32_t xor_ip = real_ip ^ NatProbe::STUN_MAGIC_COOKIE;
    resp[28] = static_cast<uint8_t>(xor_ip >> 24);
    resp[29] = static_cast<uint8_t>((xor_ip >> 16) & 0xFF);
    resp[30] = static_cast<uint8_t>((xor_ip >> 8) & 0xFF);
    resp[31] = static_cast<uint8_t>(xor_ip & 0xFF);

    StunEndpoint ep;
    bool success = NatProbe::ParseBindingResponse(resp.data(), resp.size(), tx_id, ep);
    ASSERT_TRUE(success);
    EXPECT_EQ(ep.ip, "198.51.100.42");
    EXPECT_EQ(ep.port, 51820);
}

TEST(NatProbeTest, ParseCorruptedAndMismatchedResponses) {
    std::array<uint8_t, 12> tx_id;
    Crypto::GenerateRandom(tx_id.data(), tx_id.size());

    StunEndpoint ep;
    // Too short
    std::vector<uint8_t> short_pkt(15, 0);
    EXPECT_FALSE(NatProbe::ParseBindingResponse(short_pkt.data(), short_pkt.size(), tx_id, ep));

    // Mismatched transaction ID
    auto req = NatProbe::BuildBindingRequest(tx_id);
    req[0] = 0x01; req[1] = 0x01; // Change to response
    std::array<uint8_t, 12> other_tx_id{};
    EXPECT_FALSE(NatProbe::ParseBindingResponse(req.data(), req.size(), other_tx_id, ep));
}

TEST(NatProbeTest, ClassifyNatTopologies) {
    StunEndpoint ep_a{"203.0.113.10", 40001};
    StunEndpoint ep_b{"203.0.113.10", 40001};
    StunEndpoint ep_c{"203.0.113.10", 40002}; // Different mapped port: symmetric NAT

    // Direct public IP
    EXPECT_EQ(NatProbe::ClassifyNat("203.0.113.10", ep_a, ep_b), NatType::DIRECT_PUBLIC);

    // Full Cone NAT (same external port for both probes behind private LAN)
    EXPECT_EQ(NatProbe::ClassifyNat("192.168.0.239", ep_a, ep_b), NatType::FULL_CONE);

    // Symmetric NAT (external port differs across servers)
    EXPECT_EQ(NatProbe::ClassifyNat("192.168.0.239", ep_a, ep_c), NatType::SYMMETRIC_NAT);

    // Blocked (no response from either)
    EXPECT_EQ(NatProbe::ClassifyNat("192.168.0.239", std::nullopt, std::nullopt), NatType::BLOCKED);
}

TEST(RelayClientTest, FrameAndUnframeRoundTrip) {
    std::array<uint8_t, 32> dest_key;
    Crypto::GenerateRandom(dest_key.data(), dest_key.size());

    std::vector<uint8_t> payload = {0x04, 0x00, 0x00, 0x00, 0xCA, 0xFE, 0xBA, 0xBE}; // Dummy WireGuard transport packet

    auto frame = RelayClient::FramePacket(RelayCommand::DATA, dest_key, payload.data(), payload.size());
    ASSERT_EQ(frame.size(), sizeof(RelayHeader) + payload.size());

    RelayHeader out_hdr{};
    const uint8_t* out_payload = nullptr;
    size_t out_len = 0;

    bool ok = RelayClient::UnframePacket(frame.data(), frame.size(), out_hdr, out_payload, out_len);
    ASSERT_TRUE(ok);
    EXPECT_EQ(ntohl(out_hdr.magic), RelayClient::RELAY_MAGIC);
    EXPECT_EQ(out_hdr.version, RelayClient::RELAY_VERSION);
    EXPECT_EQ(out_hdr.command, static_cast<uint8_t>(RelayCommand::DATA));
    EXPECT_EQ(std::memcmp(out_hdr.dest_key, dest_key.data(), 32), 0);
    EXPECT_EQ(out_len, payload.size());
    EXPECT_EQ(std::memcmp(out_payload, payload.data(), payload.size()), 0);
}

TEST(RelayClientTest, LifecycleAndStats) {
    std::array<uint8_t, 32> my_key;
    Crypto::GenerateRandom(my_key.data(), my_key.size());

    std::array<uint8_t, 32> dest_key;
    Crypto::GenerateRandom(dest_key.data(), dest_key.size());

    RelayClient client("relay.openroute.internal", 4433, my_key);
    EXPECT_EQ(client.GetState(), RelayState::DISCONNECTED);
    EXPECT_FALSE(client.IsConnected());

    EXPECT_TRUE(client.Connect());
    EXPECT_TRUE(client.IsConnected());
    EXPECT_EQ(client.GetState(), RelayState::REGISTERED);

    std::vector<uint8_t> wg_data = {0x04, 0x01, 0x02, 0x03, 0xAA, 0xBB};
    EXPECT_TRUE(client.SendRelayedPacket(dest_key, wg_data.data(), wg_data.size()));
    EXPECT_EQ(client.GetState(), RelayState::RELAYING);

    auto stats = client.GetStats();
    EXPECT_EQ(stats.packets_relayed_tx, 1u);
    EXPECT_EQ(stats.bytes_relayed_tx, wg_data.size());

    client.Disconnect();
    EXPECT_EQ(client.GetState(), RelayState::DISCONNECTED);
    EXPECT_FALSE(client.IsConnected());
}
