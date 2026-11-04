#pragma once

#include "core/storage/database.hpp"
#include "core/security/crypto.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace openroute::policy {

struct Invitation {
    std::string id;
    std::string gateway_id;
    std::string token_hash;
    std::string target_platform; // 'all', 'android', 'windows'
    std::string pass_type;       // 'TEMPORARY_2H', 'TEMPORARY_24H', 'TEMPORARY_7D', 'UNLIMITED'
    int max_uses = 1;
    int current_uses = 0;
    int64_t expires_at = 0;      // invitation link expiration epoch
    int64_t pass_duration_sec = 0;
    bool is_revoked = false;
    int64_t created_at = 0;
};

struct InvitationPayload {
    std::string token;           // Secret raw token (sent in QR / link)
    std::string invitation_url;  // openroute://invite?...
    std::string qr_payload;      // URI / JSON payload for QR code
    int64_t expires_at = 0;
    std::string pass_type;
};

class InvitationManager {
public:
    explicit InvitationManager(storage::Database& db);

    // Creates a new invitation token and returns QR payload
    std::optional<InvitationPayload> CreateInvitation(
        const std::string& gateway_id,
        const std::string& gateway_name,
        const std::string& gateway_pubkey_b64,
        uint16_t gateway_port,
        const std::string& target_platform = "all",
        const std::string& pass_type = "TEMPORARY_24H",
        int64_t pass_duration_sec = 86400, // 24 hours
        int64_t invite_validity_sec = 900,  // 15 mins to scan
        int max_uses = 1);

    // Validates and consumes an invitation token during client onboarding
    bool ValidateAndRedeemToken(const std::string& raw_token,
                                int64_t& out_pass_duration_sec,
                                std::string& out_gateway_id);

    bool RevokeInvitation(const std::string& invitation_id);
    std::vector<Invitation> ListActiveInvitations(const std::string& gateway_id);

    static std::string HashToken(std::string_view raw_token);

private:
    storage::Database& db_;
};

} // namespace openroute::policy
