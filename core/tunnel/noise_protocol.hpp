#pragma once

#include "core/security/crypto.hpp"
#include "core/tunnel/wireguard_types.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

namespace openroute::tunnel {

struct HandshakeState {
    security::KeyBuffer chaining_key{};
    security::HashBuffer hash{};
    security::KeyPair local_ephemeral{};
    security::KeyBuffer remote_ephemeral{};
    security::KeyBuffer remote_static{};
    uint32_t local_index = 0;
    uint32_t remote_index = 0;
    std::chrono::system_clock::time_point initiation_time{};
};

struct TransportKeys {
    security::KeyBuffer send_key{};
    security::KeyBuffer recv_key{};
    uint32_t local_index = 0;
    uint32_t remote_index = 0;
};

class NoiseProtocol {
public:
    static void InitializeConstants();

    // Initiator: creates Type 1 packet
    static bool CreateInitiation(const security::KeyPair& local_static,
                                const security::KeyBuffer& remote_static,
                                const security::KeyBuffer& preshared_key,
                                uint32_t local_index,
                                PacketHandshakeInitiation& out_packet,
                                HandshakeState& out_state);

    // Responder: consumes Type 1 packet and authenticates initiator
    static bool ConsumeInitiation(const security::KeyPair& local_static,
                                 const PacketHandshakeInitiation& packet,
                                 security::KeyBuffer& out_remote_static,
                                 HandshakeState& out_state);

    // Responder: creates Type 2 packet after consuming initiation
    static bool CreateResponse(const security::KeyPair& local_static,
                              const security::KeyBuffer& preshared_key,
                              uint32_t local_index,
                              HandshakeState& state,
                              PacketHandshakeResponse& out_packet,
                              TransportKeys& out_keys);

    // Initiator: consumes Type 2 packet and completes handshake
    static bool ConsumeResponse(const security::KeyPair& local_static,
                               const security::KeyBuffer& preshared_key,
                               const PacketHandshakeResponse& packet,
                               HandshakeState& state,
                               TransportKeys& out_keys);

    // Transport packet encryption (Type 4)
    static bool EncryptTransport(const TransportKeys& keys,
                                uint64_t counter,
                                const uint8_t* plaintext, size_t plaintext_len,
                                std::vector<uint8_t>& out_packet);

    // Transport packet decryption (Type 4)
    static bool DecryptTransport(const TransportKeys& keys,
                                const uint8_t* packet_data, size_t packet_len,
                                uint64_t& out_counter,
                                std::vector<uint8_t>& out_plaintext);

private:
    static void MixHash(security::HashBuffer& h, const uint8_t* data, size_t len);
    static void MixKey(security::KeyBuffer& ck, const uint8_t* input, size_t len);
    static void MixKeyAndDerive(security::KeyBuffer& ck, security::KeyBuffer& out_key,
                               const uint8_t* input, size_t len);
};

} // namespace openroute::tunnel
