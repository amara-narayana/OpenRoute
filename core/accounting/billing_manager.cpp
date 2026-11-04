#include "core/accounting/billing_manager.hpp"
#include "core/common/logger.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>

namespace openroute::accounting {

namespace {
int64_t GetCurrentEpoch() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}
} // namespace

BillingManager::BillingManager(storage::Database& db) : db_(db) {}

bool BillingManager::Load() {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        // 1. Load Rates
        auto stmt_rates = db_.Prepare("SELECT currency, rate_per_gb, rate_per_hour FROM billing_settings WHERE id = 'default' LIMIT 1;");
        if (stmt_rates.Step()) {
            rates_.currency = stmt_rates.GetText(0);
            rates_.rate_per_gb = stmt_rates.GetDouble(1);
            rates_.rate_per_hour = stmt_rates.GetDouble(2);
        }

        // 2. Load recent sessions
        sessions_.clear();
        auto stmt_sess = db_.Prepare(
            "SELECT session_id, dial_pin, device_name, client_ip, platform, start_time, end_time, "
            "bytes_rx, bytes_tx, duration_seconds, billed_amount, status FROM call_sessions "
            "ORDER BY start_time DESC LIMIT 100;"
        );

        while (stmt_sess.Step()) {
            CallSession s;
            s.session_id = stmt_sess.GetText(0);
            s.dial_pin = stmt_sess.GetText(1);
            s.device_name = stmt_sess.GetText(2);
            s.client_ip = stmt_sess.GetText(3);
            s.platform = stmt_sess.GetText(4);
            s.start_time = stmt_sess.GetInt64(5);
            s.end_time = stmt_sess.GetInt64(6);
            s.bytes_rx = static_cast<uint64_t>(stmt_sess.GetInt64(7));
            s.bytes_tx = static_cast<uint64_t>(stmt_sess.GetInt64(8));
            s.billed_amount = stmt_sess.GetDouble(10);
            std::string st = stmt_sess.GetText(11);
            s.is_hung_up = (st == "COMPLETED" || st == "HUNG_UP");
            s.is_paused = (st == "PAUSED");
            s.is_approved = (st != "PENDING");
            sessions_.push_back(s);
        }

        LOG_INFO << "[BillingManager] Loaded pricing rates: " << rates_.currency
                 << " " << rates_.rate_per_gb << "/GB and " << sessions_.size() << " session history records.";
        return true;
    } catch (const std::exception& ex) {
        LOG_WARN << "[BillingManager] Could not load billing records from database: " << ex.what();
        return false;
    }
}

PricingRates BillingManager::GetRates() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return rates_;
}

bool BillingManager::UpdateRates(const PricingRates& rates) {
    std::lock_guard<std::mutex> lock(mutex_);
    rates_ = rates;
    try {
        auto stmt = db_.Prepare(
            "INSERT INTO billing_settings (id, currency, rate_per_gb, rate_per_hour, updated_at) "
            "VALUES ('default', ?1, ?2, ?3, ?4) "
            "ON CONFLICT(id) DO UPDATE SET currency = ?1, rate_per_gb = ?2, rate_per_hour = ?3, updated_at = ?4;"
        );
        stmt.BindText(1, rates.currency);
        stmt.BindDouble(2, rates.rate_per_gb);
        stmt.BindDouble(3, rates.rate_per_hour);
        stmt.BindInt64(4, GetCurrentEpoch());
        stmt.Step();
        return true;
    } catch (const std::exception& ex) {
        LOG_ERROR << "[BillingManager] Failed to update pricing rates: " << ex.what();
        return false;
    }
}

double BillingManager::CalculateAmount(uint64_t total_bytes, int64_t /*duration_seconds*/) const {
    // Usage billing based on GBs consumed
    double gb = static_cast<double>(total_bytes) / (1024.0 * 1024.0 * 1024.0);
    double amount = gb * rates_.rate_per_gb;
    // Minimum charge for starting a call is 0.10 units
    if (total_bytes > 0 && amount < 0.05) {
        amount = 0.05;
    }
    return amount;
}

std::string BillingManager::StartOrResumeCall(
    const std::string& dial_pin,
    const std::string& client_ip,
    const std::string& platform,
    const std::string& device_name,
    uint64_t quota_bytes,
    int64_t expires_at)
{
    std::lock_guard<std::mutex> lock(mutex_);
    int64_t now = GetCurrentEpoch();

    // Check if an active session already exists for this PIN or client IP
    for (auto& s : sessions_) {
        if (!s.is_hung_up && (s.dial_pin == dial_pin || (!client_ip.empty() && s.client_ip == client_ip))) {
            if (!device_name.empty()) s.device_name = device_name;
            if (!platform.empty()) s.platform = platform;
            s.client_ip = client_ip;
            s.is_hung_up = false;
            PersistSession(s);
            return s.session_id;
        }
    }

    CallSession s;
    s.session_id = "call_" + std::to_string(now) + "_" + dial_pin;
    s.dial_pin = dial_pin;
    s.client_ip = client_ip;
    s.platform = platform.empty() ? "Universal" : platform;
    s.device_name = device_name.empty() ? ("Phone-" + dial_pin) : device_name;
    s.start_time = now;
    s.end_time = 0;
    s.bytes_rx = 0;
    s.bytes_tx = 0;
    s.quota_bytes = quota_bytes;
    s.expires_at = expires_at;
    s.is_approved = true;
    s.is_paused = false;
    s.is_hung_up = false;
    s.billed_amount = 0.0;

    sessions_.insert(sessions_.begin(), s);
    PersistSession(s);
    LOG_INFO << "[BillingManager] Started internet call session: " << s.session_id << " (PIN: " << dial_pin << ", IP: " << client_ip << ")";
    return s.session_id;
}

bool BillingManager::RecordTraffic(const std::string& session_id, uint64_t rx_bytes, uint64_t tx_bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& s : sessions_) {
        if (s.session_id == session_id && !s.is_hung_up) {
            s.bytes_rx += rx_bytes;
            s.bytes_tx += tx_bytes;
            int64_t duration = GetCurrentEpoch() - s.start_time;
            s.billed_amount = CalculateAmount(s.bytes_rx + s.bytes_tx, duration);
            PersistSession(s);
            return true;
        }
    }
    return false;
}

bool BillingManager::HangUpCall(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& s : sessions_) {
        if (s.session_id == session_id) {
            s.is_hung_up = true;
            s.end_time = GetCurrentEpoch();
            int64_t duration = s.end_time - s.start_time;
            s.billed_amount = CalculateAmount(s.bytes_rx + s.bytes_tx, duration);
            PersistSession(s);
            LOG_INFO << "[BillingManager] Hung up call " << session_id << ". Billed: "
                     << FormatCurrency(s.billed_amount, rates_.currency);
            return true;
        }
    }
    return false;
}

bool BillingManager::PauseCall(const std::string& session_id, bool pause) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& s : sessions_) {
        if (s.session_id == session_id) {
            s.is_paused = pause;
            PersistSession(s);
            return true;
        }
    }
    return false;
}

bool BillingManager::ApproveCall(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& s : sessions_) {
        if (s.session_id == session_id) {
            s.is_approved = true;
            s.is_paused = false;
            s.is_hung_up = false;
            PersistSession(s);
            return true;
        }
    }
    return false;
}

std::optional<CallSession> BillingManager::GetCall(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& s : sessions_) {
        if (s.session_id == session_id) return s;
    }
    return std::nullopt;
}

std::optional<CallSession> BillingManager::FindActiveCallByPin(const std::string& dial_pin) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& s : sessions_) {
        if (!s.is_hung_up && s.dial_pin == dial_pin) return s;
    }
    return std::nullopt;
}

std::optional<CallSession> BillingManager::FindCallByPin(const std::string& dial_pin) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& s : sessions_) {
        if (!s.is_hung_up && s.dial_pin == dial_pin) return s;
    }
    for (auto it = sessions_.rbegin(); it != sessions_.rend(); ++it) {
        if (it->dial_pin == dial_pin) return *it;
    }
    return std::nullopt;
}

std::optional<CallSession> BillingManager::FindActiveCallByIp(const std::string& client_ip) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& s : sessions_) {
        if (!s.is_hung_up && s.client_ip == client_ip) return s;
    }
    return std::nullopt;
}

std::vector<CallSession> BillingManager::GetActiveCalls() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<CallSession> active;
    for (const auto& s : sessions_) {
        if (!s.is_hung_up) active.push_back(s);
    }
    return active;
}

std::vector<CallSession> BillingManager::GetAllCalls() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_;
}

BillingSummary BillingManager::GetSummary() const {
    std::lock_guard<std::mutex> lock(mutex_);
    BillingSummary sum;
    sum.currency = rates_.currency;
    sum.total_sessions = static_cast<int>(sessions_.size());

    for (const auto& s : sessions_) {
        sum.total_revenue += s.billed_amount;
        sum.total_bytes += (s.bytes_rx + s.bytes_tx);
        if (!s.is_hung_up) {
            sum.active_sessions++;
        }
    }
    return sum;
}

void BillingManager::PersistSession(const CallSession& session) {
    try {
        auto stmt = db_.Prepare(
            "INSERT INTO call_sessions (session_id, dial_pin, device_name, client_ip, platform, "
            "start_time, end_time, bytes_rx, bytes_tx, duration_seconds, billed_amount, status) "
            "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12) "
            "ON CONFLICT(session_id) DO UPDATE SET "
            "end_time = ?7, bytes_rx = ?8, bytes_tx = ?9, duration_seconds = ?10, "
            "billed_amount = ?11, status = ?12;"
        );

        int64_t duration = (session.end_time > 0 ? session.end_time : GetCurrentEpoch()) - session.start_time;
        std::string status = session.is_hung_up ? "HUNG_UP" : (session.is_paused ? "PAUSED" : (!session.is_approved ? "PENDING" : "ACTIVE"));

        stmt.BindText(1, session.session_id);
        stmt.BindText(2, session.dial_pin);
        stmt.BindText(3, session.device_name);
        stmt.BindText(4, session.client_ip);
        stmt.BindText(5, session.platform);
        stmt.BindInt64(6, session.start_time);
        stmt.BindInt64(7, session.end_time);
        stmt.BindInt64(8, static_cast<int64_t>(session.bytes_rx));
        stmt.BindInt64(9, static_cast<int64_t>(session.bytes_tx));
        stmt.BindInt64(10, duration);
        stmt.BindDouble(11, session.billed_amount);
        stmt.BindText(12, status);
        stmt.Step();
    } catch (...) {}
}

std::string BillingManager::FormatBytes(uint64_t bytes) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    if (bytes < 1024) {
        ss << bytes << " B";
    } else if (bytes < 1024 * 1024) {
        ss << (static_cast<double>(bytes) / 1024.0) << " KB";
    } else if (bytes < 1024 * 1024 * 1024) {
        ss << (static_cast<double>(bytes) / (1024.0 * 1024.0)) << " MB";
    } else {
        ss << (static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0)) << " GB";
    }
    return ss.str();
}

std::string BillingManager::FormatDuration(int64_t seconds) {
    if (seconds < 0) seconds = 0;
    int hrs = static_cast<int>(seconds / 3600);
    int mins = static_cast<int>((seconds % 3600) / 60);
    int secs = static_cast<int>(seconds % 60);

    std::ostringstream ss;
    if (hrs > 0) {
        ss << std::setw(2) << std::setfill('0') << hrs << ":";
    }
    ss << std::setw(2) << std::setfill('0') << mins << ":"
       << std::setw(2) << std::setfill('0') << secs;
    return ss.str();
}

std::string BillingManager::FormatCurrency(double amount, const std::string& currency) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    if (currency == "INR") {
        ss << "\xE2\x82\xB9 " << amount; // UTF-8 Indian Rupee symbol: ₹
    } else if (currency == "USD") {
        ss << "$ " << amount;
    } else if (currency == "EUR") {
        ss << "\xE2\x82\xAC " << amount;
    } else {
        ss << currency << " " << amount;
    }
    return ss.str();
}

} // namespace openroute::accounting
