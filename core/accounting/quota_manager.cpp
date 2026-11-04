#include "core/accounting/quota_manager.hpp"
#include "core/common/logger.hpp"

#include <chrono>

namespace openroute::accounting {

QuotaManager::QuotaManager(storage::Database* db) : db_(db) {}

void QuotaManager::RegisterPeer(const PeerAccountingState& state) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = peers_.find(state.device_id);
    if (it == peers_.end()) {
        peers_[state.device_id] = state;
    } else {
        it->second.expires_at = state.expires_at;
        if (state.daily_quota_bytes > 0) it->second.daily_quota_bytes = state.daily_quota_bytes;
        if (state.monthly_quota_bytes > 0) it->second.monthly_quota_bytes = state.monthly_quota_bytes;
    }
}

void QuotaManager::UpdateLimits(const std::string& device_id, uint64_t daily_quota,
                                uint64_t monthly_quota, int64_t expires_at) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = peers_.find(device_id);
    if (it != peers_.end()) {
        it->second.daily_quota_bytes = daily_quota;
        it->second.monthly_quota_bytes = monthly_quota;
        it->second.expires_at = expires_at;
    }
}

void QuotaManager::SetPaused(const std::string& device_id, bool paused) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = peers_.find(device_id);
    if (it != peers_.end()) {
        it->second.is_paused = paused;
    }
}

void QuotaManager::SetRevoked(const std::string& device_id, bool revoked) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = peers_.find(device_id);
    if (it != peers_.end()) {
        it->second.is_revoked = revoked;
    }
}

QuotaStatus QuotaManager::CheckStatus(const std::string& device_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = peers_.find(device_id);
    if (it == peers_.end()) {
        return QuotaStatus::ExceededQuota;
    }

    const auto& peer = it->second;
    if (peer.is_revoked || peer.is_paused) {
        return QuotaStatus::ExceededQuota;
    }

    // Check expiry
    if (peer.expires_at > 0) {
        auto now_sec = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        if (now_sec >= peer.expires_at) {
            return QuotaStatus::ExpiredPass;
        }
    }

    // Check usage limits
    uint64_t total = peer.bytes_rx + peer.bytes_tx;
    if (peer.daily_quota_bytes > 0 && total >= peer.daily_quota_bytes) {
        return QuotaStatus::ExceededQuota;
    }
    if (peer.monthly_quota_bytes > 0 && total >= peer.monthly_quota_bytes) {
        return QuotaStatus::ExceededQuota;
    }

    // Check approaching limit (> 80%)
    if (peer.daily_quota_bytes > 0 && total >= (peer.daily_quota_bytes * 8 / 10)) {
        return QuotaStatus::WarningApproaching;
    }

    return QuotaStatus::WithinQuota;
}

QuotaStatus QuotaManager::RecordTraffic(const std::string& device_id, size_t rx_bytes, size_t tx_bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = peers_.find(device_id);
    if (it == peers_.end()) {
        return QuotaStatus::ExceededQuota;
    }

    it->second.bytes_rx += rx_bytes;
    it->second.bytes_tx += tx_bytes;

    uint64_t total = it->second.bytes_rx + it->second.bytes_tx;
    if (it->second.daily_quota_bytes > 0 && total >= it->second.daily_quota_bytes) {
        return QuotaStatus::ExceededQuota;
    }

    return QuotaStatus::WithinQuota;
}

uint64_t QuotaManager::GetTotalUsage(const std::string& device_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = peers_.find(device_id);
    if (it != peers_.end()) {
        return it->second.bytes_rx + it->second.bytes_tx;
    }
    return 0;
}

void QuotaManager::ResetUsage(const std::string& device_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = peers_.find(device_id);
    if (it != peers_.end()) {
        it->second.bytes_rx = 0;
        it->second.bytes_tx = 0;
    }
}

bool QuotaManager::PersistToDatabase() {
    if (!db_ || !db_->IsOpen()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto tx = db_->BeginTransaction();

    auto now_epoch = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    for (const auto& [id, peer] : peers_) {
        try {
            auto stmt = db_->Prepare(R"(
                INSERT INTO usage_records (id, device_id, timestamp_epoch, bytes_rx, bytes_tx, duration_seconds)
                VALUES (?1, ?2, ?3, ?4, ?5, ?6);
            )");
            std::string rec_id = id + "_" + std::to_string(now_epoch);
            stmt.BindText(1, rec_id);
            stmt.BindText(2, id);
            stmt.BindInt64(3, now_epoch);
            stmt.BindInt64(4, static_cast<int64_t>(peer.bytes_rx));
            stmt.BindInt64(5, static_cast<int64_t>(peer.bytes_tx));
            stmt.BindInt64(6, 60);
            stmt.Step();
        } catch (const std::exception& ex) {
            LOG_ERROR << "Failed to persist usage record for " << id << ": " << ex.what();
            return false;
        }
    }

    tx.Commit();
    return true;
}

} // namespace openroute::accounting
