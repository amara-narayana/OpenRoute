#pragma once

#include "core/storage/database.hpp"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <ostream>
#include <string>
#include <unordered_map>

namespace openroute::accounting {

enum class QuotaStatus {
    WithinQuota,
    WarningApproaching, // > 80%
    ExceededQuota,
    ExpiredPass
};

inline std::ostream& operator<<(std::ostream& os, QuotaStatus s) {
    switch (s) {
        case QuotaStatus::WithinQuota: return os << "WithinQuota";
        case QuotaStatus::WarningApproaching: return os << "WarningApproaching";
        case QuotaStatus::ExceededQuota: return os << "ExceededQuota";
        case QuotaStatus::ExpiredPass: return os << "ExpiredPass";
        default: return os << "Unknown";
    }
}

struct PeerAccountingState {
    std::string device_id;
    uint64_t bytes_rx = 0; // Downloaded by client
    uint64_t bytes_tx = 0; // Uploaded by client
    uint64_t daily_quota_bytes = 0;   // 0 = unlimited
    uint64_t monthly_quota_bytes = 0; // 0 = unlimited
    int64_t expires_at = 0;           // epoch timestamp in seconds, 0 = no expiry
    bool is_paused = false;
    bool is_revoked = false;
};

class QuotaManager {
public:
    explicit QuotaManager(storage::Database* db = nullptr);

    void RegisterPeer(const PeerAccountingState& state);
    void UpdateLimits(const std::string& device_id, uint64_t daily_quota, uint64_t monthly_quota, int64_t expires_at);
    void SetPaused(const std::string& device_id, bool paused);
    void SetRevoked(const std::string& device_id, bool revoked);

    // Records packet transmission and returns quota status
    QuotaStatus RecordTraffic(const std::string& device_id, size_t rx_bytes, size_t tx_bytes);
    [[nodiscard]] QuotaStatus CheckStatus(const std::string& device_id) const;

    [[nodiscard]] uint64_t GetTotalUsage(const std::string& device_id) const;
    void ResetUsage(const std::string& device_id);

    // Flushes in-memory accounting counters to SQLite
    bool PersistToDatabase();

private:
    storage::Database* db_ = nullptr;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, PeerAccountingState> peers_;
};

} // namespace openroute::accounting
