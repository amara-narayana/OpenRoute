#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace openroute::security {

constexpr size_t BLAKE2S_BLOCKBYTES = 64;
constexpr size_t BLAKE2S_OUTBYTES = 32;
constexpr size_t BLAKE2S_KEYBYTES = 32;

struct Blake2sState {
    uint32_t h[8];
    uint32_t t[2];
    uint32_t f[2];
    uint8_t  buf[BLAKE2S_BLOCKBYTES];
    size_t   buflen;
    size_t   outlen;
    uint8_t  last_node;
};

void Blake2sInit(Blake2sState* S, size_t outlen);
void Blake2sInitKey(Blake2sState* S, size_t outlen, const void* key, size_t keylen);
void Blake2sUpdate(Blake2sState* S, const void* in, size_t inlen);
void Blake2sFinal(Blake2sState* S, void* out);

// One-shot helper
void Blake2s(void* out, size_t outlen, const void* key, size_t keylen, const void* in, size_t inlen);

// HMAC-BLAKE2s
void HmacBlake2s(uint8_t out[32], const uint8_t* key, size_t keylen, const uint8_t* in, size_t inlen);

} // namespace openroute::security
