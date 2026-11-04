#include "tests/test_framework.hpp"
#include "core/security/crypto.hpp"
#include "core/security/blake2s.hpp"

#include <cstring>
#include <string>

using namespace openroute::security;

TEST(CryptoTest, GenerateRandom) {
    uint8_t buf1[32] = {0};
    uint8_t buf2[32] = {0};

    EXPECT_TRUE(Crypto::GenerateRandom(buf1, sizeof(buf1)));
    EXPECT_TRUE(Crypto::GenerateRandom(buf2, sizeof(buf2)));

    // Verify randomness: two 32-byte buffers must not be identical or all zeroes
    EXPECT_FALSE(Crypto::ConstantTimeEqual(buf1, buf2, 32));

    uint8_t zero[32] = {0};
    EXPECT_FALSE(Crypto::ConstantTimeEqual(buf1, zero, 32));
}

TEST(CryptoTest, X25519KeyExchange) {
    // Generate Alice keypair
    auto alice = Crypto::GenerateKeyPair();
    // Generate Bob keypair
    auto bob = Crypto::GenerateKeyPair();

    EXPECT_FALSE(Crypto::ConstantTimeEqual(alice.public_key.data(), bob.public_key.data(), 32));

    // Alice computes shared secret with Bob's public key
    KeyBuffer alice_shared{};
    EXPECT_TRUE(Crypto::ComputeSharedSecret(alice.private_key, bob.public_key, alice_shared));

    // Bob computes shared secret with Alice's public key
    KeyBuffer bob_shared{};
    EXPECT_TRUE(Crypto::ComputeSharedSecret(bob.private_key, alice.public_key, bob_shared));

    // Both shared secrets must be identical (Diffie-Hellman property)
    EXPECT_TRUE(Crypto::ConstantTimeEqual(alice_shared.data(), bob_shared.data(), 32));

    // Verify derive public key produces same public key
    auto alice_pub_derived = Crypto::DerivePublicKey(alice.private_key);
    EXPECT_TRUE(Crypto::ConstantTimeEqual(alice.public_key.data(), alice_pub_derived.data(), 32));
}

TEST(CryptoTest, Blake2sHashing) {
    const std::string msg = "OpenRoute secure internet sharing";
    auto hash1 = Crypto::Blake2s(reinterpret_cast<const uint8_t*>(msg.data()), msg.size());
    auto hash2 = Crypto::Blake2s(reinterpret_cast<const uint8_t*>(msg.data()), msg.size());

    // Deterministic hashing
    EXPECT_TRUE(Crypto::ConstantTimeEqual(hash1.data(), hash2.data(), 32));

    // Different message produces different hash
    const std::string msg_diff = "OpenRoute secure internet sharing!";
    auto hash3 = Crypto::Blake2s(reinterpret_cast<const uint8_t*>(msg_diff.data()), msg_diff.size());
    EXPECT_FALSE(Crypto::ConstantTimeEqual(hash1.data(), hash3.data(), 32));
}

TEST(CryptoTest, Blake2sKeyedMac) {
    KeyBuffer key = Crypto::GenerateRandomKey();
    const std::string payload = "Type 1 Handshake Initiation Packet";

    auto mac1 = Crypto::Blake2s(key.data(), key.size(),
                                reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
    auto mac2 = Crypto::Blake2s(key.data(), key.size(),
                                reinterpret_cast<const uint8_t*>(payload.data()), payload.size());

    EXPECT_TRUE(Crypto::ConstantTimeEqual(mac1.data(), mac2.data(), 32));

    // Altered key produces different MAC
    KeyBuffer key2 = Crypto::GenerateRandomKey();
    auto mac3 = Crypto::Blake2s(key2.data(), key2.size(),
                                reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
    EXPECT_FALSE(Crypto::ConstantTimeEqual(mac1.data(), mac3.data(), 32));
}

TEST(CryptoTest, ChaCha20Poly1305AeadRoundTrip) {
    KeyBuffer session_key = Crypto::GenerateRandomKey();
    uint64_t counter = 42;

    const std::string plaintext = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nOpenRoute Data";
    const std::string ad = "TunnelPacketHeader10.77.0.2";

    std::vector<uint8_t> ciphertext;
    TagBuffer tag{};

    EXPECT_TRUE(Crypto::AeadEncrypt(
        session_key, counter,
        reinterpret_cast<const uint8_t*>(plaintext.data()), plaintext.size(),
        reinterpret_cast<const uint8_t*>(ad.data()), ad.size(),
        ciphertext, tag));

    EXPECT_EQ(ciphertext.size(), plaintext.size());

    // Decrypt with correct key, counter, tag, and AD
    std::vector<uint8_t> decrypted;
    EXPECT_TRUE(Crypto::AeadDecrypt(
        session_key, counter,
        ciphertext.data(), ciphertext.size(),
        tag,
        reinterpret_cast<const uint8_t*>(ad.data()), ad.size(),
        decrypted));

    EXPECT_EQ(decrypted.size(), plaintext.size());
    std::string decrypted_str(reinterpret_cast<char*>(decrypted.data()), decrypted.size());
    EXPECT_EQ(decrypted_str, plaintext);

    // Tamper with ciphertext -> decryption must fail
    auto tampered = ciphertext;
    tampered[0] ^= 0xFF;
    std::vector<uint8_t> fail_plain;
    EXPECT_FALSE(Crypto::AeadDecrypt(
        session_key, counter,
        tampered.data(), tampered.size(),
        tag,
        reinterpret_cast<const uint8_t*>(ad.data()), ad.size(),
        fail_plain));

    // Tamper with tag -> decryption must fail
    auto tampered_tag = tag;
    tampered_tag[0] ^= 0x01;
    EXPECT_FALSE(Crypto::AeadDecrypt(
        session_key, counter,
        ciphertext.data(), ciphertext.size(),
        tampered_tag,
        reinterpret_cast<const uint8_t*>(ad.data()), ad.size(),
        fail_plain));

    // Wrong counter -> decryption must fail
    EXPECT_FALSE(Crypto::AeadDecrypt(
        session_key, counter + 1,
        ciphertext.data(), ciphertext.size(),
        tag,
        reinterpret_cast<const uint8_t*>(ad.data()), ad.size(),
        fail_plain));
}

TEST(CryptoTest, Base64EncodingAndDecoding) {
    KeyBuffer original = Crypto::GenerateRandomKey();
    std::string b64 = Crypto::KeyToBase64(original);

    // WireGuard base64 is 44 characters (32 bytes * 4 / 3 rounded up with padding '=')
    EXPECT_EQ(b64.size(), 44U);

    auto decoded = Crypto::KeyFromBase64(b64);
    EXPECT_TRUE(decoded.has_value());
    EXPECT_TRUE(Crypto::ConstantTimeEqual(original.data(), decoded->data(), 32));

    // Invalid base64
    EXPECT_FALSE(Crypto::KeyFromBase64("invalid_not_base64!").has_value());
}

TEST(CryptoTest, DpapiDataProtection) {
    const std::string secret = "SUPER_SECRET_GATEWAY_PRIVATE_KEY_BYTES";
    std::vector<uint8_t> encrypted;

    bool ok = Crypto::ProtectData(
        reinterpret_cast<const uint8_t*>(secret.data()), secret.size(), encrypted);
    EXPECT_TRUE(ok);
    EXPECT_GT(encrypted.size(), secret.size());

    std::vector<uint8_t> decrypted;
    ok = Crypto::UnprotectData(encrypted.data(), encrypted.size(), decrypted);
    EXPECT_TRUE(ok);

    std::string decrypted_str(reinterpret_cast<char*>(decrypted.data()), decrypted.size());
    EXPECT_EQ(decrypted_str, secret);
}
