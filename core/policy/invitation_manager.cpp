#include "core/policy/invitation_manager.hpp"
#include "core/common/logger.hpp"

#include <atomic>
#include <chrono>
#include <sstream>

namespace openroute::policy {

InvitationManager::InvitationManager(storage::Database& db) : db_(db) {}

std::string InvitationManager::HashToken(std::string_view raw_token) {
    auto hash = security::Crypto::Blake2s(
        reinterpret_cast<const uint8_t*>(raw_token.data()), raw_token.size());
    return security::Crypto::Base64Encode(hash.data(), hash.size());
}

std::optional<InvitationPayload> InvitationManager::CreateInvitation(
    const std::string& gateway_id,
    const std::string& gateway_name,
    const std::string& gateway_pubkey_b64,
    uint16_t gateway_port,
    const std::string& target_platform,
    const std::string& pass_type,
    int64_t pass_duration_sec,
    int64_t invite_validity_sec,
    int max_uses) {

    auto raw_key = security::Crypto::GenerateRandomKey();
    std::string raw_token = security::Crypto::KeyToBase64(raw_key);
    std::string token_hash = HashToken(raw_token);

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    int64_t expires_at = now + invite_validity_sec;

    static std::atomic<uint64_t> s_inv_seq{0};
    std::string inv_id = "inv_" + std::to_string(now) + "_" + std::to_string(++s_inv_seq);

    try {
        auto stmt = db_.Prepare(R"(
            INSERT INTO invitations (id, gateway_id, token_hash, target_platform, pass_type, max_uses, current_uses, expires_at, is_revoked, created_at)
            VALUES (?1, ?2, ?3, ?4, ?5, ?6, 0, ?7, 0, ?8);
        )");
        stmt.BindText(1, inv_id);
        stmt.BindText(2, gateway_id);
        stmt.BindText(3, token_hash);
        stmt.BindText(4, target_platform);
        stmt.BindText(5, pass_type);
        stmt.BindInt(6, max_uses);
        stmt.BindInt64(7, expires_at);
        stmt.BindInt64(8, now);
        stmt.Step();

        // Construct invitation URL & QR code payload
        std::ostringstream ss;
        ss << "openroute://invite?"
           << "token=" << raw_token
           << "&gw=" << gateway_id
           << "&name=" << gateway_name
           << "&pub=" << gateway_pubkey_b64
           << "&port=" << gateway_port
           << "&pass=" << pass_type
           << "&dur=" << pass_duration_sec;

        InvitationPayload payload;
        payload.token = raw_token;
        payload.invitation_url = ss.str();
        payload.qr_payload = ss.str();
        payload.expires_at = expires_at;
        payload.pass_type = pass_type;

        LOG_INFO << "Generated invitation " << inv_id << " (expires in " << invite_validity_sec << "s)";
        return payload;
    } catch (const std::exception& ex) {
        LOG_ERROR << "Failed to create invitation: " << ex.what();
        return std::nullopt;
    }
}

bool InvitationManager::ValidateAndRedeemToken(const std::string& raw_token,
                                              int64_t& out_pass_duration_sec,
                                              std::string& out_gateway_id) {
    std::string token_hash = HashToken(raw_token);
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    try {
        auto stmt = db_.Prepare(R"(
            SELECT id, gateway_id, pass_type, max_uses, current_uses, expires_at, is_revoked
            FROM invitations WHERE token_hash = ?1;
        )");
        stmt.BindText(1, token_hash);

        if (!stmt.Step()) {
            LOG_WARN << "Invalid invitation token presented";
            return false;
        }

        std::string inv_id = stmt.GetText(0);
        out_gateway_id = stmt.GetText(1);
        std::string pass_type = stmt.GetText(2);
        int max_uses = stmt.GetInt(3);
        int current_uses = stmt.GetInt(4);
        int64_t expires_at = stmt.GetInt64(5);
        int is_revoked = stmt.GetInt(6);

        if (is_revoked != 0) {
            LOG_WARN << "Attempted redemption of revoked invitation: " << inv_id;
            return false;
        }

        if (now > expires_at) {
            LOG_WARN << "Attempted redemption of expired invitation: " << inv_id;
            return false;
        }

        if (current_uses >= max_uses) {
            LOG_WARN << "Invitation uses exceeded: " << inv_id;
            return false;
        }

        // Determine pass duration
        if (pass_type == "TEMPORARY_2H") {
            out_pass_duration_sec = 2 * 3600;
        } else if (pass_type == "TEMPORARY_24H") {
            out_pass_duration_sec = 24 * 3600;
        } else if (pass_type == "TEMPORARY_7D") {
            out_pass_duration_sec = 7 * 86400;
        } else {
            out_pass_duration_sec = 0; // unlimited
        }

        // Increment current_uses
        auto update_stmt = db_.Prepare("UPDATE invitations SET current_uses = current_uses + 1 WHERE id = ?1;");
        update_stmt.BindText(1, inv_id);
        update_stmt.Step();

        LOG_INFO << "Invitation " << inv_id << " successfully redeemed.";
        return true;
    } catch (const std::exception& ex) {
        LOG_ERROR << "Failed to validate invitation: " << ex.what();
        return false;
    }
}

bool InvitationManager::RevokeInvitation(const std::string& invitation_id) {
    try {
        auto stmt = db_.Prepare("UPDATE invitations SET is_revoked = 1 WHERE id = ?1;");
        stmt.BindText(1, invitation_id);
        stmt.Step();
        LOG_INFO << "Invitation revoked: " << invitation_id;
        return true;
    } catch (const std::exception& ex) {
        LOG_ERROR << "Failed to revoke invitation " << invitation_id << ": " << ex.what();
        return false;
    }
}

std::vector<Invitation> InvitationManager::ListActiveInvitations(const std::string& gateway_id) {
    std::vector<Invitation> list;
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    try {
        auto stmt = db_.Prepare(R"(
            SELECT id, gateway_id, token_hash, target_platform, pass_type, max_uses, current_uses, expires_at, is_revoked, created_at
            FROM invitations
            WHERE gateway_id = ?1 AND is_revoked = 0 AND expires_at > ?2
            ORDER BY created_at DESC;
        )");
        stmt.BindText(1, gateway_id);
        stmt.BindInt64(2, now);

        while (stmt.Step()) {
            Invitation inv;
            inv.id = stmt.GetText(0);
            inv.gateway_id = stmt.GetText(1);
            inv.token_hash = stmt.GetText(2);
            inv.target_platform = stmt.GetText(3);
            inv.pass_type = stmt.GetText(4);
            inv.max_uses = stmt.GetInt(5);
            inv.current_uses = stmt.GetInt(6);
            inv.expires_at = stmt.GetInt64(7);
            inv.is_revoked = (stmt.GetInt(8) != 0);
            inv.created_at = stmt.GetInt64(9);
            list.push_back(inv);
        }
    } catch (...) {}
    return list;
}

} // namespace openroute::policy
