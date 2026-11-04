#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace openroute::security {

constexpr size_t kKeySize = 32;       // 256-bit X25519 / ChaCha20 / BLAKE2s key
constexpr size_t kAeadTagSize = 16;   // 128-bit Poly1305 authentication tag
constexpr size_t kHashSize = 32;      // 256-bit BLAKE2s hash

using KeyBuffer = std::array<uint8_t, kKeySize>;
using TagBuffer = std::array<uint8_t, kAeadTagSize>;
using HashBuffer = std::array<uint8_t, kHashSize>;

struct KeyPair {
    KeyBuffer private_key{};
    KeyBuffer public_key{};

    ~KeyPair(); // Securely wipes private key memory upon destruction
};

class Crypto {
public:
    // Secure random generation
    static bool GenerateRandom(uint8_t* out, size_t size);
    static KeyBuffer GenerateRandomKey();

    // Key generation & ECDH (Curve25519 / X25519)
    static KeyPair GenerateKeyPair();
    static KeyBuffer DerivePublicKey(const KeyBuffer& private_key);
    static bool ComputeSharedSecret(const KeyBuffer& my_private_key,
                                   const KeyBuffer& peer_public_key,
                                   KeyBuffer& out_shared_secret);

    // BLAKE2s-256 Hashing & MAC
    static HashBuffer Blake2s(const uint8_t* data, size_t size);
    static HashBuffer Blake2s(const uint8_t* key, size_t key_len,
                              const uint8_t* data, size_t data_len);

    // HKDF-BLAKE2s
    static void Hkdf1(const KeyBuffer& key, const uint8_t* input, size_t input_len,
                      KeyBuffer& out1);
    static void Hkdf2(const KeyBuffer& key, const uint8_t* input, size_t input_len,
                      KeyBuffer& out1, KeyBuffer& out2);
    static void Hkdf3(const KeyBuffer& key, const uint8_t* input, size_t input_len,
                      KeyBuffer& out1, KeyBuffer& out2, KeyBuffer& out3);

    // Authenticated Encryption with Associated Data (ChaCha20-Poly1305)
    // Nonce is 64-bit integer, zero-padded to 12 or 24 bytes per WireGuard / IETF spec
    static bool AeadEncrypt(const KeyBuffer& key, uint64_t counter,
                            const uint8_t* plaintext, size_t plaintext_len,
                            const uint8_t* ad, size_t ad_len,
                            std::vector<uint8_t>& out_ciphertext,
                            TagBuffer& out_tag);

    static bool AeadDecrypt(const KeyBuffer& key, uint64_t counter,
                            const uint8_t* ciphertext, size_t ciphertext_len,
                            const TagBuffer& tag,
                            const uint8_t* ad, size_t ad_len,
                            std::vector<uint8_t>& out_plaintext);

    // Constant-time memory wipe & comparison
    static void Wipe(void* data, size_t size);
    static bool ConstantTimeEqual(const uint8_t* a, const uint8_t* b, size_t size);

    // Base64 encoding & decoding (standard WireGuard format)
    static std::string Base64Encode(const uint8_t* data, size_t len);
    static std::optional<std::vector<uint8_t>> Base64Decode(std::string_view base64);
    static std::string KeyToBase64(const KeyBuffer& key);
    static std::optional<KeyBuffer> KeyFromBase64(std::string_view base64);

    // Windows DPAPI Protection for At-Rest Credentials
    static bool ProtectData(const uint8_t* plaintext, size_t len,
                            std::vector<uint8_t>& out_encrypted);
    static bool UnprotectData(const uint8_t* ciphertext, size_t len,
                              std::vector<uint8_t>& out_decrypted);
};

} // namespace openroute::security
