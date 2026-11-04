#include "core/tunnel/noise_protocol.hpp"
#include "core/common/logger.hpp"
#include "core/security/blake2s.hpp"

#include <chrono>
#include <cstring>

namespace openroute::tunnel {

namespace {

static const char kConstructionId[] = "Noise_IKpsk2_25519_ChaChaPoly_BLAKE2s";
static const char kIdentifier[] = "WireGuard v1 zx2c4 Jason@zx2c4.com";
static const char kLabelMac1[] = "mac1----";

static security::KeyBuffer s_initial_ck{};
static security::HashBuffer s_initial_h{};
static bool s_constants_initialized = false;

inline void store32_le(uint8_t* dst, uint32_t val) {
    dst[0] = static_cast<uint8_t>(val);
    dst[1] = static_cast<uint8_t>(val >> 8);
    dst[2] = static_cast<uint8_t>(val >> 16);
    dst[3] = static_cast<uint8_t>(val >> 24);
}

inline uint32_t load32_le(const uint8_t* src) {
    return static_cast<uint32_t>(src[0]) |
          (static_cast<uint32_t>(src[1]) << 8) |
          (static_cast<uint32_t>(src[2]) << 16) |
          (static_cast<uint32_t>(src[3]) << 24);
}

inline void store64_le(uint8_t* dst, uint64_t val) {
    for (size_t i = 0; i < 8; ++i) {
        dst[i] = static_cast<uint8_t>((val >> (8 * i)) & 0xFF);
    }
}

inline uint64_t load64_le(const uint8_t* src) {
    uint64_t val = 0;
    for (size_t i = 0; i < 8; ++i) {
        val |= (static_cast<uint64_t>(src[i]) << (8 * i));
    }
    return val;
}

// Generate a 12-byte TAI64N timestamp
void GenerateTai64n(uint8_t out[12]) {
    auto now = std::chrono::system_clock::now();
    auto epoch_sec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    auto epoch_nano = (std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count()) % 1'000'000'000ULL;

    uint64_t tai_sec = static_cast<uint64_t>(epoch_sec) + 0x400000000000000aULL;
    for (int i = 7; i >= 0; --i) {
        out[i] = static_cast<uint8_t>(tai_sec & 0xFF);
        tai_sec >>= 8;
    }
    uint32_t nano = static_cast<uint32_t>(epoch_nano);
    for (int i = 11; i >= 8; --i) {
        out[i] = static_cast<uint8_t>(nano & 0xFF);
        nano >>= 8;
    }
}

void ComputeMac1(const security::KeyBuffer& remote_pub,
                const uint8_t* packet_data, size_t len,
                uint8_t out_mac1[16]) {
    // mac1_key = BLAKE2s(kLabelMac1 || remote_pub)
    uint8_t key_material[8 + security::kKeySize];
    std::memcpy(key_material, kLabelMac1, 8);
    std::memcpy(key_material + 8, remote_pub.data(), security::kKeySize);

    security::HashBuffer mac1_key = security::Crypto::Blake2s(key_material, sizeof(key_material));
    security::HashBuffer full_mac = security::Crypto::Blake2s(mac1_key.data(), mac1_key.size(),
                                                              packet_data, len);
    std::memcpy(out_mac1, full_mac.data(), 16);
}

} // namespace

void NoiseProtocol::InitializeConstants() {
    if (s_constants_initialized) return;

    // CK_0 = BLAKE2s(kConstructionId)
    s_initial_ck = security::Crypto::Blake2s(reinterpret_cast<const uint8_t*>(kConstructionId),
                                             std::strlen(kConstructionId));

    // H_0 = BLAKE2s(CK_0 || kIdentifier)
    uint8_t combo[security::kKeySize + 34];
    std::memcpy(combo, s_initial_ck.data(), security::kKeySize);
    std::memcpy(combo + security::kKeySize, kIdentifier, std::strlen(kIdentifier));
    s_initial_h = security::Crypto::Blake2s(combo, sizeof(combo));

    s_constants_initialized = true;
}

void NoiseProtocol::MixHash(security::HashBuffer& h, const uint8_t* data, size_t len) {
    std::vector<uint8_t> combo(h.size() + len);
    std::memcpy(combo.data(), h.data(), h.size());
    std::memcpy(combo.data() + h.size(), data, len);
    h = security::Crypto::Blake2s(combo.data(), combo.size());
}

void NoiseProtocol::MixKey(security::KeyBuffer& ck, const uint8_t* input, size_t len) {
    security::KeyBuffer out_ck{};
    security::Crypto::Hkdf1(ck, input, len, out_ck);
    ck = out_ck;
}

void NoiseProtocol::MixKeyAndDerive(security::KeyBuffer& ck, security::KeyBuffer& out_key,
                                   const uint8_t* input, size_t len) {
    security::KeyBuffer next_ck{};
    security::Crypto::Hkdf2(ck, input, len, next_ck, out_key);
    ck = next_ck;
}

bool NoiseProtocol::CreateInitiation(const security::KeyPair& local_static,
                                     const security::KeyBuffer& remote_static,
                                     const security::KeyBuffer& /*preshared_key*/,
                                     uint32_t local_index,
                                     PacketHandshakeInitiation& out_packet,
                                     HandshakeState& out_state) {
    InitializeConstants();

    out_state.chaining_key = s_initial_ck;
    out_state.hash = s_initial_h;
    out_state.local_index = local_index;
    out_state.remote_static = remote_static;
    out_state.local_ephemeral = security::Crypto::GenerateKeyPair();
    out_state.initiation_time = std::chrono::system_clock::now();

    // H = MixHash(H, remote_static)
    MixHash(out_state.hash, remote_static.data(), remote_static.size());

    // Ephemeral
    std::memcpy(out_packet.ephemeral_public, out_state.local_ephemeral.public_key.data(), 32);
    MixHash(out_state.hash, out_packet.ephemeral_public, 32);
    MixKey(out_state.chaining_key, out_packet.ephemeral_public, 32);

    // SS1 = X25519(local_ephemeral.priv, remote_static)
    security::KeyBuffer ss1{};
    if (!security::Crypto::ComputeSharedSecret(out_state.local_ephemeral.private_key,
                                               remote_static, ss1)) {
        return false;
    }

    security::KeyBuffer key1{};
    MixKeyAndDerive(out_state.chaining_key, key1, ss1.data(), ss1.size());

    // Encrypt static
    std::vector<uint8_t> enc_static;
    security::TagBuffer tag_static{};
    security::Crypto::AeadEncrypt(key1, 0, local_static.public_key.data(), 32,
                                  out_state.hash.data(), out_state.hash.size(),
                                  enc_static, tag_static);

    std::memcpy(out_packet.encrypted_static, enc_static.data(), 32);
    std::memcpy(out_packet.encrypted_static + 32, tag_static.data(), 16);
    MixHash(out_state.hash, out_packet.encrypted_static, sizeof(out_packet.encrypted_static));

    // SS2 = X25519(local_static.priv, remote_static)
    security::KeyBuffer ss2{};
    if (!security::Crypto::ComputeSharedSecret(local_static.private_key,
                                               remote_static, ss2)) {
        return false;
    }

    security::KeyBuffer key2{};
    MixKeyAndDerive(out_state.chaining_key, key2, ss2.data(), ss2.size());

    // Encrypt timestamp
    uint8_t timestamp[12];
    GenerateTai64n(timestamp);
    std::vector<uint8_t> enc_ts;
    security::TagBuffer tag_ts{};
    security::Crypto::AeadEncrypt(key2, 0, timestamp, sizeof(timestamp),
                                  out_state.hash.data(), out_state.hash.size(),
                                  enc_ts, tag_ts);

    std::memcpy(out_packet.encrypted_timestamp, enc_ts.data(), 12);
    std::memcpy(out_packet.encrypted_timestamp + 12, tag_ts.data(), 16);
    MixHash(out_state.hash, out_packet.encrypted_timestamp, sizeof(out_packet.encrypted_timestamp));

    // Populate header
    out_packet.type = static_cast<uint32_t>(PacketType::HandshakeInitiation);
    out_packet.sender_index = local_index;

    // MAC1
    ComputeMac1(remote_static, reinterpret_cast<const uint8_t*>(&out_packet), 116, out_packet.mac1);

    // MAC2 (zeroed initially until cookie requested)
    std::memset(out_packet.mac2, 0, 16);

    return true;
}

bool NoiseProtocol::ConsumeInitiation(const security::KeyPair& local_static,
                                      const PacketHandshakeInitiation& packet,
                                      security::KeyBuffer& out_remote_static,
                                      HandshakeState& out_state) {
    InitializeConstants();

    if (packet.type != static_cast<uint32_t>(PacketType::HandshakeInitiation)) {
        return false;
    }

    // Verify MAC1
    uint8_t expected_mac1[16];
    ComputeMac1(local_static.public_key, reinterpret_cast<const uint8_t*>(&packet), 116, expected_mac1);
    if (!security::Crypto::ConstantTimeEqual(packet.mac1, expected_mac1, 16)) {
        LOG_WARN << "Invalid MAC1 in Handshake Initiation packet";
        return false;
    }

    out_state.chaining_key = s_initial_ck;
    out_state.hash = s_initial_h;
    out_state.remote_index = packet.sender_index;

    // MixHash(H, local_static.pub)
    MixHash(out_state.hash, local_static.public_key.data(), local_static.public_key.size());

    // MixHash(H, packet.ephemeral)
    std::memcpy(out_state.remote_ephemeral.data(), packet.ephemeral_public, 32);
    MixHash(out_state.hash, packet.ephemeral_public, 32);
    MixKey(out_state.chaining_key, packet.ephemeral_public, 32);

    // SS1 = X25519(local_static.priv, packet.ephemeral)
    security::KeyBuffer ss1{};
    if (!security::Crypto::ComputeSharedSecret(local_static.private_key,
                                               out_state.remote_ephemeral, ss1)) {
        return false;
    }

    security::KeyBuffer key1{};
    MixKeyAndDerive(out_state.chaining_key, key1, ss1.data(), ss1.size());

    // Decrypt static
    security::TagBuffer tag_static{};
    std::memcpy(tag_static.data(), packet.encrypted_static + 32, 16);

    std::vector<uint8_t> plain_static;
    if (!security::Crypto::AeadDecrypt(key1, 0, packet.encrypted_static, 32, tag_static,
                                       out_state.hash.data(), out_state.hash.size(),
                                       plain_static) || plain_static.size() != 32) {
        LOG_WARN << "Failed to decrypt static key in Handshake Initiation";
        return false;
    }

    std::memcpy(out_remote_static.data(), plain_static.data(), 32);
    out_state.remote_static = out_remote_static;
    MixHash(out_state.hash, packet.encrypted_static, sizeof(packet.encrypted_static));

    // SS2 = X25519(local_static.priv, remote_static)
    security::KeyBuffer ss2{};
    if (!security::Crypto::ComputeSharedSecret(local_static.private_key,
                                               out_remote_static, ss2)) {
        return false;
    }

    security::KeyBuffer key2{};
    MixKeyAndDerive(out_state.chaining_key, key2, ss2.data(), ss2.size());

    // Decrypt timestamp
    security::TagBuffer tag_ts{};
    std::memcpy(tag_ts.data(), packet.encrypted_timestamp + 12, 16);

    std::vector<uint8_t> plain_ts;
    if (!security::Crypto::AeadDecrypt(key2, 0, packet.encrypted_timestamp, 12, tag_ts,
                                       out_state.hash.data(), out_state.hash.size(),
                                       plain_ts) || plain_ts.size() != 12) {
        LOG_WARN << "Failed to decrypt timestamp in Handshake Initiation";
        return false;
    }

    MixHash(out_state.hash, packet.encrypted_timestamp, sizeof(packet.encrypted_timestamp));
    return true;
}

bool NoiseProtocol::CreateResponse(const security::KeyPair& /*local_static*/,
                                   const security::KeyBuffer& preshared_key,
                                   uint32_t local_index,
                                   HandshakeState& state,
                                   PacketHandshakeResponse& out_packet,
                                   TransportKeys& out_keys) {
    state.local_index = local_index;
    state.local_ephemeral = security::Crypto::GenerateKeyPair();

    std::memcpy(out_packet.ephemeral_public, state.local_ephemeral.public_key.data(), 32);
    MixHash(state.hash, out_packet.ephemeral_public, 32);
    MixKey(state.chaining_key, out_packet.ephemeral_public, 32);

    // SS3 = X25519(local_ephemeral.priv, remote_ephemeral)
    security::KeyBuffer ss3{};
    if (!security::Crypto::ComputeSharedSecret(state.local_ephemeral.private_key,
                                               state.remote_ephemeral, ss3)) {
        return false;
    }
    MixKey(state.chaining_key, ss3.data(), ss3.size());

    // SS4 = X25519(local_ephemeral.priv, remote_static)
    security::KeyBuffer ss4{};
    if (!security::Crypto::ComputeSharedSecret(state.local_ephemeral.private_key,
                                               state.remote_static, ss4)) {
        return false;
    }
    MixKey(state.chaining_key, ss4.data(), ss4.size());

    // PSK mix: (CK, tau, key) = HKDF3(CK, PSK)
    security::KeyBuffer tau{}, key{};
    security::Crypto::Hkdf3(state.chaining_key, preshared_key.data(), preshared_key.size(),
                            state.chaining_key, tau, key);
    MixHash(state.hash, tau.data(), tau.size());

    // Encrypt empty
    std::vector<uint8_t> enc_empty;
    security::TagBuffer tag_empty{};
    security::Crypto::AeadEncrypt(key, 0, nullptr, 0,
                                  state.hash.data(), state.hash.size(),
                                  enc_empty, tag_empty);

    std::memcpy(out_packet.encrypted_nothing, tag_empty.data(), 16);
    MixHash(state.hash, out_packet.encrypted_nothing, sizeof(out_packet.encrypted_nothing));

    out_packet.type = static_cast<uint32_t>(PacketType::HandshakeResponse);
    out_packet.sender_index = local_index;
    out_packet.receiver_index = state.remote_index;

    // MAC1
    ComputeMac1(state.remote_static, reinterpret_cast<const uint8_t*>(&out_packet), 60, out_packet.mac1);
    std::memset(out_packet.mac2, 0, 16);

    // Derive transport keys: for responder, key1 = send_key, key2 = recv_key
    security::Crypto::Hkdf2(state.chaining_key, nullptr, 0, out_keys.send_key, out_keys.recv_key);
    out_keys.local_index = local_index;
    out_keys.remote_index = state.remote_index;

    return true;
}

bool NoiseProtocol::ConsumeResponse(const security::KeyPair& local_static,
                                    const security::KeyBuffer& preshared_key,
                                    const PacketHandshakeResponse& packet,
                                    HandshakeState& state,
                                    TransportKeys& out_keys) {
    if (packet.type != static_cast<uint32_t>(PacketType::HandshakeResponse)) {
        return false;
    }

    // Verify MAC1
    uint8_t expected_mac1[16];
    ComputeMac1(local_static.public_key, reinterpret_cast<const uint8_t*>(&packet), 60, expected_mac1);
    if (!security::Crypto::ConstantTimeEqual(packet.mac1, expected_mac1, 16)) {
        LOG_WARN << "Invalid MAC1 in Handshake Response packet";
        return false;
    }

    std::memcpy(state.remote_ephemeral.data(), packet.ephemeral_public, 32);
    MixHash(state.hash, packet.ephemeral_public, 32);
    MixKey(state.chaining_key, packet.ephemeral_public, 32);

    // SS3 = X25519(local_ephemeral.priv, remote_ephemeral)
    security::KeyBuffer ss3{};
    if (!security::Crypto::ComputeSharedSecret(state.local_ephemeral.private_key,
                                               state.remote_ephemeral, ss3)) {
        return false;
    }
    MixKey(state.chaining_key, ss3.data(), ss3.size());

    // SS4 = X25519(local_static.priv, remote_ephemeral)
    security::KeyBuffer ss4{};
    if (!security::Crypto::ComputeSharedSecret(local_static.private_key,
                                               state.remote_ephemeral, ss4)) {
        return false;
    }
    MixKey(state.chaining_key, ss4.data(), ss4.size());

    // PSK mix
    security::KeyBuffer tau{}, key{};
    security::Crypto::Hkdf3(state.chaining_key, preshared_key.data(), preshared_key.size(),
                            state.chaining_key, tau, key);
    MixHash(state.hash, tau.data(), tau.size());

    // Decrypt empty
    security::TagBuffer tag_empty{};
    std::memcpy(tag_empty.data(), packet.encrypted_nothing, 16);

    std::vector<uint8_t> plain_empty;
    if (!security::Crypto::AeadDecrypt(key, 0, nullptr, 0, tag_empty,
                                       state.hash.data(), state.hash.size(),
                                       plain_empty)) {
        LOG_WARN << "Failed to decrypt encrypted_nothing in Handshake Response";
        return false;
    }

    state.remote_index = packet.sender_index;

    // Derive transport keys: for initiator, key1 = recv_key, key2 = send_key
    security::Crypto::Hkdf2(state.chaining_key, nullptr, 0, out_keys.recv_key, out_keys.send_key);
    out_keys.local_index = state.local_index;
    out_keys.remote_index = packet.sender_index;

    return true;
}

bool NoiseProtocol::EncryptTransport(const TransportKeys& keys,
                                     uint64_t counter,
                                     const uint8_t* plaintext, size_t plaintext_len,
                                     std::vector<uint8_t>& out_packet) {
    out_packet.resize(kTransportHeaderSize + plaintext_len + kTagSize);

    auto* hdr = reinterpret_cast<PacketTransportHeader*>(out_packet.data());
    hdr->type = static_cast<uint32_t>(PacketType::TransportData);
    hdr->receiver_index = keys.remote_index;
    hdr->counter = counter;

    std::vector<uint8_t> ciphertext;
    security::TagBuffer tag{};

    if (!security::Crypto::AeadEncrypt(keys.send_key, counter,
                                       plaintext, plaintext_len,
                                       nullptr, 0,
                                       ciphertext, tag)) {
        return false;
    }

    std::memcpy(out_packet.data() + kTransportHeaderSize, ciphertext.data(), ciphertext.size());
    std::memcpy(out_packet.data() + kTransportHeaderSize + ciphertext.size(), tag.data(), kTagSize);
    return true;
}

bool NoiseProtocol::DecryptTransport(const TransportKeys& keys,
                                     const uint8_t* packet_data, size_t packet_len,
                                     uint64_t& out_counter,
                                     std::vector<uint8_t>& out_plaintext) {
    if (packet_len < kMinTransportPacket) {
        return false;
    }

    const auto* hdr = reinterpret_cast<const PacketTransportHeader*>(packet_data);
    if (hdr->type != static_cast<uint32_t>(PacketType::TransportData)) {
        return false;
    }

    out_counter = hdr->counter;
    size_t cipher_len = packet_len - kTransportHeaderSize - kTagSize;

    security::TagBuffer tag{};
    std::memcpy(tag.data(), packet_data + packet_len - kTagSize, kTagSize);

    return security::Crypto::AeadDecrypt(keys.recv_key, out_counter,
                                         packet_data + kTransportHeaderSize, cipher_len,
                                         tag, nullptr, 0, out_plaintext);
}

} // namespace openroute::tunnel
