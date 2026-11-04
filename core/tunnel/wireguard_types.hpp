#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace openroute::tunnel {

// WireGuard packet type indicators
enum class PacketType : uint32_t {
    HandshakeInitiation = 1,
    HandshakeResponse   = 2,
    CookieReply         = 3,
    TransportData       = 4
};

constexpr size_t kInitiationPacketSize = 148;
constexpr size_t kResponsePacketSize   = 92;
constexpr size_t kCookiePacketSize     = 64;
constexpr size_t kTransportHeaderSize  = 16; // 4 (type) + 4 (receiver) + 8 (counter)
constexpr size_t kTagSize              = 16;
constexpr size_t kMinTransportPacket   = kTransportHeaderSize + kTagSize; // 32 bytes (keepalive)

#pragma pack(push, 1)

// Type 1: Handshake Initiation (148 bytes)
struct PacketHandshakeInitiation {
    uint32_t type;                             // 1
    uint32_t sender_index;                     // chosen by initiator
    uint8_t  ephemeral_public[32];             // unencrypted ephemeral public key
    uint8_t  encrypted_static[32 + kTagSize];  // static public key encrypted
    uint8_t  encrypted_timestamp[12 + kTagSize]; // TAI64N timestamp encrypted
    uint8_t  mac1[16];                         // BLAKE2s(responder_pub, packet[0..116])
    uint8_t  mac2[16];                         // BLAKE2s(cookie, packet[0..132])
};
static_assert(sizeof(PacketHandshakeInitiation) == kInitiationPacketSize, "Invalid Initiation packet size");

// Type 2: Handshake Response (92 bytes)
struct PacketHandshakeResponse {
    uint32_t type;                             // 2
    uint32_t sender_index;                     // chosen by responder
    uint32_t receiver_index;                   // initiator sender_index
    uint8_t  ephemeral_public[32];             // responder ephemeral public key
    uint8_t  encrypted_nothing[kTagSize];      // empty AEAD payload
    uint8_t  mac1[16];                         // BLAKE2s(initiator_pub, packet[0..60])
    uint8_t  mac2[16];                         // BLAKE2s(cookie, packet[0..76])
};
static_assert(sizeof(PacketHandshakeResponse) == kResponsePacketSize, "Invalid Response packet size");

// Type 3: Cookie Reply (64 bytes)
struct PacketCookieReply {
    uint32_t type;                             // 3
    uint32_t receiver_index;                   // initiator / sender index
    uint8_t  nonce[24];                        // random 24-byte nonce
    uint8_t  encrypted_cookie[16 + kTagSize];  // 16-byte cookie encrypted with XChaCha20-Poly1305
};
static_assert(sizeof(PacketCookieReply) == kCookiePacketSize, "Invalid Cookie packet size");

// Type 4: Transport Data Packet Header (16 bytes)
struct PacketTransportHeader {
    uint32_t type;           // 4
    uint32_t receiver_index; // session index of recipient
    uint64_t counter;        // monotonically increasing sequence number
};
static_assert(sizeof(PacketTransportHeader) == kTransportHeaderSize, "Invalid Transport header size");

#pragma pack(pop)

inline PacketType GetPacketType(const uint8_t* buffer, size_t len) {
    if (len < 4) return static_cast<PacketType>(0);
    uint32_t type = static_cast<uint32_t>(buffer[0]) |
                   (static_cast<uint32_t>(buffer[1]) << 8) |
                   (static_cast<uint32_t>(buffer[2]) << 16) |
                   (static_cast<uint32_t>(buffer[3]) << 24);
    return static_cast<PacketType>(type);
}

} // namespace openroute::tunnel
