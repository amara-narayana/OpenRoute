#include "tests/test_framework.hpp"
#include "core/security/crypto.hpp"
#include "core/tunnel/wireguard_types.hpp"
#include "core/tunnel/replay_window.hpp"
#include "core/tunnel/noise_protocol.hpp"
#include "core/tunnel/peer_session.hpp"

#include <cstring>
#include <string>
#include <vector>

using namespace openroute::security;
using namespace openroute::tunnel;

TEST(TunnelTest, ReplayWindowValidation) {
    ReplayWindow window;

    // Normal sequential packets
    EXPECT_TRUE(window.ValidateAndMark(0));
    EXPECT_TRUE(window.ValidateAndMark(1));
    EXPECT_TRUE(window.ValidateAndMark(2));

    // Duplicate packets must be rejected
    EXPECT_FALSE(window.ValidateAndMark(1));
    EXPECT_FALSE(window.ValidateAndMark(2));
    EXPECT_FALSE(window.ValidateAndMark(0));

    // Out of order packet within window
    EXPECT_TRUE(window.ValidateAndMark(5));
    EXPECT_TRUE(window.ValidateAndMark(4));
    EXPECT_TRUE(window.ValidateAndMark(3));

    // Replay of out-of-order packet
    EXPECT_FALSE(window.ValidateAndMark(4));

    // Large jump forward
    EXPECT_TRUE(window.ValidateAndMark(200));

    // Packets older than 128 behind highest (200 - 128 = 72) must be rejected
    EXPECT_FALSE(window.ValidateAndMark(70));
    EXPECT_FALSE(window.ValidateAndMark(5));

    // Packets within window (e.g. 200 - 10 = 190) should be accepted once
    EXPECT_TRUE(window.ValidateAndMark(190));
    EXPECT_FALSE(window.ValidateAndMark(190));
}

TEST(TunnelTest, FullNoiseHandshakeAndTransport) {
    // 1. Setup Static Keypairs for Gateway (Responder) and Client (Initiator)
    auto gateway_static = Crypto::GenerateKeyPair();
    auto client_static = Crypto::GenerateKeyPair();
    KeyBuffer preshared_key = Crypto::GenerateRandomKey();

    uint32_t client_index = 0x11223344;
    uint32_t gateway_index = 0x55667788;

    // 2. Client creates Handshake Initiation (Type 1)
    PacketHandshakeInitiation init_pkt{};
    HandshakeState client_hs_state{};
    bool ok = NoiseProtocol::CreateInitiation(
        client_static, gateway_static.public_key, preshared_key,
        client_index, init_pkt, client_hs_state);

    EXPECT_TRUE(ok);
    EXPECT_EQ(init_pkt.type, static_cast<uint32_t>(PacketType::HandshakeInitiation));
    EXPECT_EQ(init_pkt.sender_index, client_index);

    // 3. Gateway consumes Handshake Initiation
    KeyBuffer auth_client_pub{};
    HandshakeState gateway_hs_state{};
    ok = NoiseProtocol::ConsumeInitiation(
        gateway_static, init_pkt, auth_client_pub, gateway_hs_state);

    EXPECT_TRUE(ok);
    // Gateway authenticated client's static public key
    EXPECT_TRUE(Crypto::ConstantTimeEqual(auth_client_pub.data(), client_static.public_key.data(), 32));
    EXPECT_EQ(gateway_hs_state.remote_index, client_index);

    // 4. Gateway creates Handshake Response (Type 2)
    PacketHandshakeResponse resp_pkt{};
    TransportKeys gateway_transport_keys{};
    ok = NoiseProtocol::CreateResponse(
        gateway_static, preshared_key, gateway_index,
        gateway_hs_state, resp_pkt, gateway_transport_keys);

    EXPECT_TRUE(ok);
    EXPECT_EQ(resp_pkt.type, static_cast<uint32_t>(PacketType::HandshakeResponse));
    EXPECT_EQ(resp_pkt.sender_index, gateway_index);
    EXPECT_EQ(resp_pkt.receiver_index, client_index);

    // 5. Client consumes Handshake Response
    TransportKeys client_transport_keys{};
    ok = NoiseProtocol::ConsumeResponse(
        client_static, preshared_key, resp_pkt,
        client_hs_state, client_transport_keys);

    EXPECT_TRUE(ok);
    EXPECT_EQ(client_transport_keys.remote_index, gateway_index);

    // 6. Verify derived transport keys match
    // Client send_key must equal Gateway recv_key
    EXPECT_TRUE(Crypto::ConstantTimeEqual(
        client_transport_keys.send_key.data(),
        gateway_transport_keys.recv_key.data(), 32));

    // Gateway send_key must equal Client recv_key
    EXPECT_TRUE(Crypto::ConstantTimeEqual(
        gateway_transport_keys.send_key.data(),
        client_transport_keys.recv_key.data(), 32));

    // 7. Test Type 4 Transport: Client -> Gateway
    const std::string client_ip_packet = "Simulated IPv4 packet: GET / HTTP/1.1";
    std::vector<uint8_t> client_transport_pkt;

    ok = NoiseProtocol::EncryptTransport(
        client_transport_keys, 0,
        reinterpret_cast<const uint8_t*>(client_ip_packet.data()), client_ip_packet.size(),
        client_transport_pkt);

    EXPECT_TRUE(ok);
    EXPECT_EQ(client_transport_pkt.size(), kTransportHeaderSize + client_ip_packet.size() + kTagSize);

    // Gateway decrypts
    uint64_t gw_rx_counter = 0;
    std::vector<uint8_t> gw_decrypted;
    ok = NoiseProtocol::DecryptTransport(
        gateway_transport_keys,
        client_transport_pkt.data(), client_transport_pkt.size(),
        gw_rx_counter, gw_decrypted);

    EXPECT_TRUE(ok);
    EXPECT_EQ(gw_rx_counter, 0ULL);
    std::string gw_dec_str(reinterpret_cast<char*>(gw_decrypted.data()), gw_decrypted.size());
    EXPECT_EQ(gw_dec_str, client_ip_packet);

    // 8. Test Type 4 Transport: Gateway -> Client (Return Path)
    const std::string gw_ip_packet = "Simulated IPv4 packet: HTTP/1.1 200 OK (Internet response)";
    std::vector<uint8_t> gw_transport_pkt;

    ok = NoiseProtocol::EncryptTransport(
        gateway_transport_keys, 0,
        reinterpret_cast<const uint8_t*>(gw_ip_packet.data()), gw_ip_packet.size(),
        gw_transport_pkt);

    EXPECT_TRUE(ok);

    // Client decrypts
    uint64_t client_rx_counter = 0;
    std::vector<uint8_t> client_decrypted;
    ok = NoiseProtocol::DecryptTransport(
        client_transport_keys,
        gw_transport_pkt.data(), gw_transport_pkt.size(),
        client_rx_counter, client_decrypted);

    EXPECT_TRUE(ok);
    EXPECT_EQ(client_rx_counter, 0ULL);
    std::string client_dec_str(reinterpret_cast<char*>(client_decrypted.data()), client_decrypted.size());
    EXPECT_EQ(client_dec_str, gw_ip_packet);
}

TEST(TunnelTest, PeerSessionEncapsulationAndReplayDefense) {
    auto gw_static = Crypto::GenerateKeyPair();
    auto client_static = Crypto::GenerateKeyPair();

    PeerSession client_peer("client_1", client_static.public_key, "10.77.0.2");
    PeerSession gw_peer("gateway_1", gw_static.public_key, "10.77.0.1");

    // Perform handshake
    PacketHandshakeInitiation init_pkt{};
    HandshakeState client_hs{};
    NoiseProtocol::CreateInitiation(client_static, gw_static.public_key, {}, 101, init_pkt, client_hs);

    KeyBuffer remote_pub{};
    HandshakeState gw_hs{};
    NoiseProtocol::ConsumeInitiation(gw_static, init_pkt, remote_pub, gw_hs);

    PacketHandshakeResponse resp_pkt{};
    TransportKeys gw_keys{};
    NoiseProtocol::CreateResponse(gw_static, {}, 202, gw_hs, resp_pkt, gw_keys);

    TransportKeys client_keys{};
    NoiseProtocol::ConsumeResponse(client_static, {}, resp_pkt, client_hs, client_keys);

    gw_peer.CompleteHandshake(gw_keys);
    client_peer.CompleteHandshake(client_keys);

    EXPECT_EQ(gw_peer.state(), SessionState::Established);
    EXPECT_EQ(client_peer.state(), SessionState::Established);

    // Send packet from client to gateway
    const std::string payload = "TCP SYN packet 10.77.0.2:443";
    std::vector<uint8_t> wire_pkt;
    EXPECT_TRUE(client_peer.Encrypt(
        reinterpret_cast<const uint8_t*>(payload.data()), payload.size(), wire_pkt));

    // Gateway decrypts
    std::vector<uint8_t> decrypted;
    EXPECT_TRUE(gw_peer.Decrypt(wire_pkt.data(), wire_pkt.size(), decrypted));
    EXPECT_EQ(std::string(reinterpret_cast<char*>(decrypted.data()), decrypted.size()), payload);

    // Replay attack: resending identical packet to gateway must fail anti-replay check
    std::vector<uint8_t> replay_decrypted;
    EXPECT_FALSE(gw_peer.Decrypt(wire_pkt.data(), wire_pkt.size(), replay_decrypted));

    // Bit-flip tampering attack
    std::vector<uint8_t> wire_pkt2;
    EXPECT_TRUE(client_peer.Encrypt(
        reinterpret_cast<const uint8_t*>(payload.data()), payload.size(), wire_pkt2));
    wire_pkt2[wire_pkt2.size() - 1] ^= 0x01; // flip MAC bit

    std::vector<uint8_t> tampered_decrypted;
    EXPECT_FALSE(gw_peer.Decrypt(wire_pkt2.data(), wire_pkt2.size(), tampered_decrypted));
}
