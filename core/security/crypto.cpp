#include "core/security/crypto.hpp"
#include "core/security/blake2s.hpp"
#include "third_party/monocypher/monocypher.h"

#if defined(_WIN32)
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Bcrypt.lib")
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include <cstring>
#include <stdexcept>

namespace openroute::security {

namespace {

inline void store64_le(uint8_t* dst, uint64_t w) {
    for (size_t i = 0; i < 8; ++i) {
        dst[i] = static_cast<uint8_t>((w >> (8 * i)) & 0xFF);
    }
}

// RFC 4648 Base64 characters
static const char kBase64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

} // namespace

KeyPair::~KeyPair() {
    Crypto::Wipe(private_key.data(), private_key.size());
}

bool Crypto::GenerateRandom(uint8_t* out, size_t size) {
    if (!out || size == 0) return false;
#if defined(_WIN32)
    NTSTATUS status = BCryptGenRandom(nullptr, out, static_cast<ULONG>(size),
                                      BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return BCRYPT_SUCCESS(status);
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) return false;
    ssize_t n = read(fd, out, size);
    close(fd);
    return n == static_cast<ssize_t>(size);
#endif
}

KeyBuffer Crypto::GenerateRandomKey() {
    KeyBuffer key{};
    GenerateRandom(key.data(), key.size());
    return key;
}

KeyPair Crypto::GenerateKeyPair() {
    KeyPair kp;
    GenerateRandom(kp.private_key.data(), kp.private_key.size());
    // Clamp private key according to Curve25519 specification
    kp.private_key[0] &= 248;
    kp.private_key[31] &= 127;
    kp.private_key[31] |= 64;

    crypto_x25519_public_key(kp.public_key.data(), kp.private_key.data());
    return kp;
}

KeyBuffer Crypto::DerivePublicKey(const KeyBuffer& private_key) {
    KeyBuffer pub{};
    crypto_x25519_public_key(pub.data(), private_key.data());
    return pub;
}

bool Crypto::ComputeSharedSecret(const KeyBuffer& my_private_key,
                                 const KeyBuffer& peer_public_key,
                                 KeyBuffer& out_shared_secret) {
    crypto_x25519(out_shared_secret.data(), my_private_key.data(), peer_public_key.data());
    // Verify it is not all zeroes (contributory behavior check)
    static const KeyBuffer kZeroes{};
    return !ConstantTimeEqual(out_shared_secret.data(), kZeroes.data(), kKeySize);
}

HashBuffer Crypto::Blake2s(const uint8_t* data, size_t size) {
    HashBuffer hash{};
    security::Blake2s(hash.data(), kHashSize, nullptr, 0, data, size);
    return hash;
}

HashBuffer Crypto::Blake2s(const uint8_t* key, size_t key_len,
                           const uint8_t* data, size_t data_len) {
    HashBuffer hash{};
    security::Blake2s(hash.data(), kHashSize, key, key_len, data, data_len);
    return hash;
}

void Crypto::Hkdf1(const KeyBuffer& key, const uint8_t* input, size_t input_len,
                   KeyBuffer& out1) {
    HashBuffer temp_hash{};
    ::openroute::security::Blake2s(temp_hash.data(), kHashSize, key.data(), key.size(), input, input_len);

    uint8_t info1 = 0x01;
    ::openroute::security::Blake2s(out1.data(), kKeySize, temp_hash.data(), temp_hash.size(), &info1, 1);
}

void Crypto::Hkdf2(const KeyBuffer& key, const uint8_t* input, size_t input_len,
                   KeyBuffer& out1, KeyBuffer& out2) {
    HashBuffer temp_hash{};
    ::openroute::security::Blake2s(temp_hash.data(), kHashSize, key.data(), key.size(), input, input_len);

    uint8_t info1 = 0x01;
    ::openroute::security::Blake2s(out1.data(), kKeySize, temp_hash.data(), temp_hash.size(), &info1, 1);

    uint8_t buf2[kKeySize + 1];
    std::memcpy(buf2, out1.data(), kKeySize);
    buf2[kKeySize] = 0x02;
    ::openroute::security::Blake2s(out2.data(), kKeySize, temp_hash.data(), temp_hash.size(), buf2, kKeySize + 1);
}

void Crypto::Hkdf3(const KeyBuffer& key, const uint8_t* input, size_t input_len,
                   KeyBuffer& out1, KeyBuffer& out2, KeyBuffer& out3) {
    HashBuffer temp_hash{};
    ::openroute::security::Blake2s(temp_hash.data(), kHashSize, key.data(), key.size(), input, input_len);

    uint8_t info1 = 0x01;
    ::openroute::security::Blake2s(out1.data(), kKeySize, temp_hash.data(), temp_hash.size(), &info1, 1);

    uint8_t buf2[kKeySize + 1];
    std::memcpy(buf2, out1.data(), kKeySize);
    buf2[kKeySize] = 0x02;
    ::openroute::security::Blake2s(out2.data(), kKeySize, temp_hash.data(), temp_hash.size(), buf2, kKeySize + 1);

    uint8_t buf3[kKeySize + 1];
    std::memcpy(buf3, out2.data(), kKeySize);
    buf3[kKeySize] = 0x03;
    ::openroute::security::Blake2s(out3.data(), kKeySize, temp_hash.data(), temp_hash.size(), buf3, kKeySize + 1);
}

bool Crypto::AeadEncrypt(const KeyBuffer& key, uint64_t counter,
                         const uint8_t* plaintext, size_t plaintext_len,
                         const uint8_t* ad, size_t ad_len,
                         std::vector<uint8_t>& out_ciphertext,
                         TagBuffer& out_tag) {
    uint8_t nonce[12] = {0};
    // WireGuard nonce format: 4 zero bytes followed by 8 bytes little-endian counter
    store64_le(nonce + 4, counter);

    out_ciphertext.resize(plaintext_len);

    crypto_aead_ctx ctx;
    crypto_aead_init_ietf(&ctx, key.data(), nonce);
    crypto_aead_write(&ctx, out_ciphertext.data(), out_tag.data(),
                      ad, ad_len, plaintext, plaintext_len);

    return true;
}

bool Crypto::AeadDecrypt(const KeyBuffer& key, uint64_t counter,
                         const uint8_t* ciphertext, size_t ciphertext_len,
                         const TagBuffer& tag,
                         const uint8_t* ad, size_t ad_len,
                         std::vector<uint8_t>& out_plaintext) {
    uint8_t nonce[12] = {0};
    store64_le(nonce + 4, counter);

    out_plaintext.resize(ciphertext_len);

    crypto_aead_ctx ctx;
    crypto_aead_init_ietf(&ctx, key.data(), nonce);
    int ok = crypto_aead_read(&ctx, out_plaintext.data(), tag.data(),
                              ad, ad_len, ciphertext, ciphertext_len);

    if (ok != 0) {
        out_plaintext.clear();
        return false;
    }
    return true;
}

void Crypto::Wipe(void* data, size_t size) {
    if (data && size > 0) {
        crypto_wipe(data, size);
    }
}

bool Crypto::ConstantTimeEqual(const uint8_t* a, const uint8_t* b, size_t size) {
    if (!a || !b) return a == b;
    if (size == 16) return crypto_verify16(a, b) == 0;
    if (size == 32) return crypto_verify32(a, b) == 0;
    if (size == 64) return crypto_verify64(a, b) == 0;

    uint8_t diff = 0;
    for (size_t i = 0; i < size; ++i) {
        diff |= static_cast<uint8_t>(a[i] ^ b[i]);
    }
    return diff == 0;
}

std::string Crypto::Base64Encode(const uint8_t* data, size_t len) {
    std::string out;
    out.reserve(((len + 2) / 3) * 4);

    for (size_t i = 0; i < len; i += 3) {
        uint32_t val = (static_cast<uint32_t>(data[i]) << 16);
        if (i + 1 < len) val |= (static_cast<uint32_t>(data[i + 1]) << 8);
        if (i + 2 < len) val |= static_cast<uint32_t>(data[i + 2]);

        out.push_back(kBase64Chars[(val >> 18) & 0x3F]);
        out.push_back(kBase64Chars[(val >> 12) & 0x3F]);
        out.push_back((i + 1 < len) ? kBase64Chars[(val >> 6) & 0x3F] : '=');
        out.push_back((i + 2 < len) ? kBase64Chars[val & 0x3F] : '=');
    }
    return out;
}

std::optional<std::vector<uint8_t>> Crypto::Base64Decode(std::string_view b64) {
    std::vector<uint8_t> out;
    out.reserve(b64.size() * 3 / 4);

    int val = 0, valb = -8;
    for (char c : b64) {
        if (c == '=') break;
        int d = -1;
        if (c >= 'A' && c <= 'Z') d = c - 'A';
        else if (c >= 'a' && c <= 'z') d = c - 'a' + 26;
        else if (c >= '0' && c <= '9') d = c - '0' + 52;
        else if (c == '+') d = 62;
        else if (c == '/') d = 63;
        else if (std::isspace(static_cast<unsigned char>(c))) continue;
        else return std::nullopt;

        val = (val << 6) + d;
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

std::string Crypto::KeyToBase64(const KeyBuffer& key) {
    return Base64Encode(key.data(), key.size());
}

std::optional<KeyBuffer> Crypto::KeyFromBase64(std::string_view base64) {
    auto dec = Base64Decode(base64);
    if (!dec || dec->size() != kKeySize) {
        return std::nullopt;
    }
    KeyBuffer key{};
    std::memcpy(key.data(), dec->data(), kKeySize);
    return key;
}

bool Crypto::ProtectData(const uint8_t* plaintext, size_t len,
                         std::vector<uint8_t>& out_encrypted) {
#if defined(_WIN32)
    DATA_BLOB in_blob;
    in_blob.pbData = const_cast<BYTE*>(plaintext);
    in_blob.cbData = static_cast<DWORD>(len);

    DATA_BLOB out_blob{};
    if (CryptProtectData(&in_blob, L"OpenRoute Key Protection", nullptr, nullptr, nullptr,
                         CRYPTPROTECT_UI_FORBIDDEN, &out_blob)) {
        out_encrypted.assign(out_blob.pbData, out_blob.pbData + out_blob.cbData);
        LocalFree(out_blob.pbData);
        return true;
    }
    return false;
#else
    // Fallback on non-Windows platforms (e.g. Android Keystore)
    out_encrypted.assign(plaintext, plaintext + len);
    return true;
#endif
}

bool Crypto::UnprotectData(const uint8_t* ciphertext, size_t len,
                           std::vector<uint8_t>& out_decrypted) {
#if defined(_WIN32)
    DATA_BLOB in_blob;
    in_blob.pbData = const_cast<BYTE*>(ciphertext);
    in_blob.cbData = static_cast<DWORD>(len);

    DATA_BLOB out_blob{};
    if (CryptUnprotectData(&in_blob, nullptr, nullptr, nullptr, nullptr,
                           CRYPTPROTECT_UI_FORBIDDEN, &out_blob)) {
        out_decrypted.assign(out_blob.pbData, out_blob.pbData + out_blob.cbData);
        LocalFree(out_blob.pbData);
        return true;
    }
    return false;
#else
    out_decrypted.assign(ciphertext, ciphertext + len);
    return true;
#endif
}

} // namespace openroute::security
