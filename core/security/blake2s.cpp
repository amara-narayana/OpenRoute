#include "core/security/blake2s.hpp"
#include <cstring>

namespace openroute::security {

namespace {

static const uint32_t blake2s_IV[8] = {
    0x6A09E667UL, 0xBB67AE85UL, 0x3C6EF372UL, 0xA54FF53AUL,
    0x510E527FUL, 0x9B05688CUL, 0x1F83D9ABUL, 0x5BE0CD19UL
};

static const uint8_t blake2s_sigma[10][16] = {
    {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
    { 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 },
    { 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4 },
    {  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8 },
    {  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13 },
    {  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9 },
    { 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11 },
    { 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10 },
    {  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5 },
    { 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13,  0 },
};

inline uint32_t rotr32(uint32_t w, unsigned c) {
    return (w >> c) | (w << (32 - c));
}

inline uint32_t load32_le(const uint8_t* src) {
    return static_cast<uint32_t>(src[0]) |
          (static_cast<uint32_t>(src[1]) << 8) |
          (static_cast<uint32_t>(src[2]) << 16) |
          (static_cast<uint32_t>(src[3]) << 24);
}

inline void store32_le(uint8_t* dst, uint32_t w) {
    dst[0] = static_cast<uint8_t>(w);
    dst[1] = static_cast<uint8_t>(w >> 8);
    dst[2] = static_cast<uint8_t>(w >> 16);
    dst[3] = static_cast<uint8_t>(w >> 24);
}

inline void G(uint32_t* v, int a, int b, int c, int d, uint32_t x, uint32_t y) {
    v[a] = v[a] + v[b] + x;
    v[d] = rotr32(v[d] ^ v[a], 16);
    v[c] = v[c] + v[d];
    v[b] = rotr32(v[b] ^ v[c], 12);
    v[a] = v[a] + v[b] + y;
    v[d] = rotr32(v[d] ^ v[a], 8);
    v[c] = v[c] + v[d];
    v[b] = rotr32(v[b] ^ v[c], 7);
}

void blake2s_compress(Blake2sState* S, const uint8_t block[BLAKE2S_BLOCKBYTES]) {
    uint32_t m[16];
    uint32_t v[16];

    for (size_t i = 0; i < 16; ++i) {
        m[i] = load32_le(block + i * 4);
    }
    for (size_t i = 0; i < 8; ++i) {
        v[i] = S->h[i];
        v[i + 8] = blake2s_IV[i];
    }

    v[12] ^= S->t[0];
    v[13] ^= S->t[1];
    v[14] ^= S->f[0];
    v[15] ^= S->f[1];

    for (size_t r = 0; r < 10; ++r) {
        G(v, 0, 4,  8, 12, m[blake2s_sigma[r][0]],  m[blake2s_sigma[r][1]]);
        G(v, 1, 5,  9, 13, m[blake2s_sigma[r][2]],  m[blake2s_sigma[r][3]]);
        G(v, 2, 6, 10, 14, m[blake2s_sigma[r][4]],  m[blake2s_sigma[r][5]]);
        G(v, 3, 7, 11, 15, m[blake2s_sigma[r][6]],  m[blake2s_sigma[r][7]]);
        G(v, 0, 5, 10, 15, m[blake2s_sigma[r][8]],  m[blake2s_sigma[r][9]]);
        G(v, 1, 6, 11, 12, m[blake2s_sigma[r][10]], m[blake2s_sigma[r][11]]);
        G(v, 2, 7,  8, 13, m[blake2s_sigma[r][12]], m[blake2s_sigma[r][13]]);
        G(v, 3, 4,  9, 14, m[blake2s_sigma[r][14]], m[blake2s_sigma[r][15]]);
    }

    for (size_t i = 0; i < 8; ++i) {
        S->h[i] ^= v[i] ^ v[i + 8];
    }
}

} // namespace

void Blake2sInit(Blake2sState* S, size_t outlen) {
    if (outlen == 0 || outlen > BLAKE2S_OUTBYTES) return;
    std::memset(S, 0, sizeof(Blake2sState));
    for (size_t i = 0; i < 8; ++i) {
        S->h[i] = blake2s_IV[i];
    }
    // Set parameter block: digest length in first byte, fanout/depth in next bytes
    S->h[0] ^= 0x01010000 ^ static_cast<uint32_t>(outlen);
    S->outlen = outlen;
}

void Blake2sInitKey(Blake2sState* S, size_t outlen, const void* key, size_t keylen) {
    if (outlen == 0 || outlen > BLAKE2S_OUTBYTES || keylen > BLAKE2S_KEYBYTES) return;
    std::memset(S, 0, sizeof(Blake2sState));
    for (size_t i = 0; i < 8; ++i) {
        S->h[i] = blake2s_IV[i];
    }
    S->h[0] ^= 0x01010000 ^ (static_cast<uint32_t>(keylen) << 8) ^ static_cast<uint32_t>(outlen);
    S->outlen = outlen;

    if (keylen > 0) {
        uint8_t block[BLAKE2S_BLOCKBYTES];
        std::memset(block, 0, BLAKE2S_BLOCKBYTES);
        std::memcpy(block, key, keylen);
        Blake2sUpdate(S, block, BLAKE2S_BLOCKBYTES);
    }
}

void Blake2sUpdate(Blake2sState* S, const void* pin, size_t inlen) {
    const auto* in = static_cast<const uint8_t*>(pin);
    while (inlen > 0) {
        size_t left = S->buflen;
        size_t fill = BLAKE2S_BLOCKBYTES - left;
        if (inlen > fill) {
            S->buflen = 0;
            std::memcpy(S->buf + left, in, fill);
            S->t[0] += BLAKE2S_BLOCKBYTES;
            if (S->t[0] < BLAKE2S_BLOCKBYTES) S->t[1]++;
            blake2s_compress(S, S->buf);
            in += fill;
            inlen -= fill;
        } else {
            std::memcpy(S->buf + left, in, inlen);
            S->buflen += inlen;
            inlen = 0;
        }
    }
}

void Blake2sFinal(Blake2sState* S, void* out) {
    S->t[0] += static_cast<uint32_t>(S->buflen);
    if (S->t[0] < S->buflen) S->t[1]++;
    S->f[0] = ~0U;
    std::memset(S->buf + S->buflen, 0, BLAKE2S_BLOCKBYTES - S->buflen);
    blake2s_compress(S, S->buf);

    uint8_t buffer[BLAKE2S_OUTBYTES];
    for (size_t i = 0; i < 8; ++i) {
        store32_le(buffer + i * 4, S->h[i]);
    }
    std::memcpy(out, buffer, S->outlen);
}

void Blake2s(void* out, size_t outlen, const void* key, size_t keylen, const void* in, size_t inlen) {
    Blake2sState S;
    if (key && keylen > 0) {
        Blake2sInitKey(&S, outlen, key, keylen);
    } else {
        Blake2sInit(&S, outlen);
    }
    Blake2sUpdate(&S, in, inlen);
    Blake2sFinal(&S, out);
}

void HmacBlake2s(uint8_t out[32], const uint8_t* key, size_t keylen, const uint8_t* in, size_t inlen) {
    // If keylen <= 32, Blake2s keyed mode is directly equivalent and standard
    if (keylen <= BLAKE2S_KEYBYTES) {
        Blake2s(out, 32, key, keylen, in, inlen);
    } else {
        uint8_t hashed_key[32];
        Blake2s(hashed_key, 32, nullptr, 0, key, keylen);
        Blake2s(out, 32, hashed_key, 32, in, inlen);
    }
}

} // namespace openroute::security
