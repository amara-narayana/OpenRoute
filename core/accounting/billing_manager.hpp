#pragma once

#include "core/storage/database.hpp"

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace openroute::accounting {

struct PricingRates {
    std::string currency = "INR"; // "INR", "USD", etc.
    double rate_per_gb = 50.0;     // e.g. 50 INR per GB
    double rate_per_hour = 20.0;   // e.g. 20 INR per hour
};

struct CallSession {
    std::string session_id;
    std::string dial_pin;
    std::string device_name;
    std::string client_ip;
    std::string platform;          // "iPhone", "Android", "Windows", "Mac", "Universal"
    int64_t start_time = 0;        // epoch seconds
    int64_t end_time = 0;          // epoch seconds (0 if active)
    uint64_t bytes_rx = 0;
    uint64_t bytes_tx = 0;
    uint64_t quota_bytes = 0;      // 0 = unlimited
    int64_t expires_at = 0;        // 0 = unlimited
    bool is_approved = true;
    bool is_paused = false;
    bool is_hung_up = false;
    double billed_amount = 0.0;
};

struct BillingSummary {
    double total_revenue = 0.0;
    uint64_t total_bytes = 0;
    int total_sessions = 0;
    int active_sessions = 0;
    std::string currency = "INR";
};

class BillingManager {
public:
    explicit BillingManager(storage::Database& db);

    bool Load();

    // Rates configuration
    PricingRates GetRates() const;
    bool UpdateRates(const PricingRates& rates);

    // Call / Session Lifecycle
    std::string StartOrResumeCall(const std::string& dial_pin,
                                  const std::string& client_ip,
                                  const std::string& platform,
                                  const std::string& device_name = "",
                                  uint64_t quota_bytes = 0,
                                  int64_t expires_at = 0);

    bool RecordTraffic(const std::string& session_id, uint64_t rx_bytes, uint64_t tx_bytes);
    bool HangUpCall(const std::string& session_id);
    bool PauseCall(const std::string& session_id, bool pause);
    bool ApproveCall(const std::string& session_id);

    std::optional<CallSession> GetCall(const std::string& session_id) const;
    std::optional<CallSession> FindActiveCallByPin(const std::string& dial_pin) const;
    std::optional<CallSession> FindActiveCallByIp(const std::string& client_ip) const;

    std::vector<CallSession> GetActiveCalls() const;
    std::vector<CallSession> GetAllCalls() const;

    BillingSummary GetSummary() const;

    // Helper formatters
    static std::string FormatBytes(uint64_t bytes);
    static std::string FormatDuration(int64_t seconds);
    static std::string FormatCurrency(double amount, const std::string& currency = "INR");

private:
    double CalculateAmount(uint64_t total_bytes, int64_t duration_seconds) const;
    void PersistSession(const CallSession& session);

    storage::Database& db_;
    mutable std::mutex mutex_;
    PricingRates rates_;
    std::vector<CallSession> sessions_;
};

} // namespace openroute::accounting
