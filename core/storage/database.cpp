#include "core/storage/database.hpp"
#include "core/common/logger.hpp"
#include "third_party/sqlite3/sqlite3.h"

#include <stdexcept>

namespace openroute::storage {

// ---------------- Statement ----------------
Statement::Statement(sqlite3* db, std::string_view sql) {
    if (!db) {
        throw std::runtime_error("Database handle is null when preparing statement");
    }
    int rc = sqlite3_prepare_v2(db, sql.data(), static_cast<int>(sql.size()), &stmt_, nullptr);
    if (rc != SQLITE_OK) {
        std::string err = sqlite3_errmsg(db);
        throw std::runtime_error("Failed to prepare statement [" + std::string(sql) + "]: " + err);
    }
}

Statement::~Statement() {
    if (stmt_) {
        sqlite3_finalize(stmt_);
        stmt_ = nullptr;
    }
}

Statement::Statement(Statement&& other) noexcept : stmt_(other.stmt_) {
    other.stmt_ = nullptr;
}

Statement& Statement::operator=(Statement&& other) noexcept {
    if (this != &other) {
        if (stmt_) {
            sqlite3_finalize(stmt_);
        }
        stmt_ = other.stmt_;
        other.stmt_ = nullptr;
    }
    return *this;
}

void Statement::BindInt(int index, int val) {
    sqlite3_bind_int(stmt_, index, val);
}

void Statement::BindInt64(int index, int64_t val) {
    sqlite3_bind_int64(stmt_, index, val);
}

void Statement::BindDouble(int index, double val) {
    sqlite3_bind_double(stmt_, index, val);
}

void Statement::BindText(int index, std::string_view text) {
    sqlite3_bind_text(stmt_, index, text.data(), static_cast<int>(text.size()), SQLITE_TRANSIENT);
}

void Statement::BindBlob(int index, const void* data, size_t size) {
    sqlite3_bind_blob(stmt_, index, data, static_cast<int>(size), SQLITE_TRANSIENT);
}

void Statement::BindNull(int index) {
    sqlite3_bind_null(stmt_, index);
}

bool Statement::Step() {
    int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) {
        return true;
    }
    if (rc == SQLITE_DONE) {
        return false;
    }
    throw std::runtime_error(std::string("SQLite step error: ") + sqlite3_errstr(rc));
}

void Statement::Reset() {
    sqlite3_reset(stmt_);
    sqlite3_clear_bindings(stmt_);
}

int Statement::GetInt(int col) {
    return sqlite3_column_int(stmt_, col);
}

int64_t Statement::GetInt64(int col) {
    return sqlite3_column_int64(stmt_, col);
}

double Statement::GetDouble(int col) {
    return sqlite3_column_double(stmt_, col);
}

std::string Statement::GetText(int col) {
    const unsigned char* text = sqlite3_column_text(stmt_, col);
    if (!text) return "";
    int bytes = sqlite3_column_bytes(stmt_, col);
    return std::string(reinterpret_cast<const char*>(text), bytes);
}

std::vector<uint8_t> Statement::GetBlob(int col) {
    const void* blob = sqlite3_column_blob(stmt_, col);
    int bytes = sqlite3_column_bytes(stmt_, col);
    if (!blob || bytes <= 0) return {};
    const auto* p = static_cast<const uint8_t*>(blob);
    return std::vector<uint8_t>(p, p + bytes);
}

bool Statement::IsNull(int col) {
    return sqlite3_column_type(stmt_, col) == SQLITE_NULL;
}

// ---------------- Transaction ----------------
Transaction::Transaction(sqlite3* db) : db_(db) {
    if (db_) {
        sqlite3_exec(db_, "BEGIN IMMEDIATE TRANSACTION;", nullptr, nullptr, nullptr);
    }
}

Transaction::~Transaction() {
    if (!committed_ && db_) {
        Rollback();
    }
}

void Transaction::Commit() {
    if (db_ && !committed_) {
        sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
        committed_ = true;
    }
}

void Transaction::Rollback() {
    if (db_ && !committed_) {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        committed_ = true;
    }
}

// ---------------- Database ----------------
Database::Database() = default;

Database::~Database() {
    Close();
}

bool Database::Open(const std::string& filepath) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    Close();

    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    int rc = sqlite3_open_v2(filepath.c_str(), &db_, flags, nullptr);
    if (rc != SQLITE_OK) {
        LOG_ERROR << "Failed to open SQLite database at " << filepath << ": " << sqlite3_errstr(rc);
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return false;
    }

    // Enable WAL mode for high concurrency and performance
    Execute("PRAGMA journal_mode=WAL;");
    Execute("PRAGMA synchronous=NORMAL;");
    Execute("PRAGMA foreign_keys=ON;");
    Execute("PRAGMA busy_timeout=5000;");

    LOG_INFO << "Opened SQLite database: " << filepath;
    return true;
}

void Database::Close() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (db_) {
        sqlite3_close_v2(db_);
        db_ = nullptr;
    }
}

bool Database::Execute(std::string_view sql) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!db_) return false;
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.data(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string err = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        LOG_ERROR << "Database execute failed [" << sql << "]: " << err;
        return false;
    }
    return true;
}

Statement Database::Prepare(std::string_view sql) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return Statement(db_, sql);
}

Transaction Database::BeginTransaction() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return Transaction(db_);
}

int Database::GetSchemaVersion() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!db_) return 0;
    try {
        auto stmt = Prepare("SELECT version FROM schema_migrations ORDER BY version DESC LIMIT 1;");
        if (stmt.Step()) {
            return stmt.GetInt(0);
        }
    } catch (...) {
        // Table may not exist yet
    }
    return 0;
}

bool Database::ApplyMigrations() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!db_) return false;

    // Base migration table
    Execute(R"(
        CREATE TABLE IF NOT EXISTS schema_migrations (
            version INTEGER PRIMARY KEY,
            applied_at INTEGER NOT NULL
        );
    )");

    int current_version = GetSchemaVersion();
    LOG_INFO << "Current database schema version: " << current_version;

    if (current_version < 1) {
        auto tx = BeginTransaction();
        bool ok = Execute(R"(
            CREATE TABLE IF NOT EXISTS gateways (
                id TEXT PRIMARY KEY,
                name TEXT NOT NULL,
                public_key TEXT NOT NULL UNIQUE,
                encrypted_private_key BLOB NOT NULL,
                listen_port INTEGER NOT NULL DEFAULT 51820,
                tunnel_subnet TEXT NOT NULL DEFAULT '10.77.0.0/24',
                egress_interface TEXT NOT NULL DEFAULT 'Wi-Fi',
                is_active INTEGER NOT NULL DEFAULT 1,
                created_at INTEGER NOT NULL,
                updated_at INTEGER NOT NULL
            );

            CREATE TABLE IF NOT EXISTS devices (
                id TEXT PRIMARY KEY,
                gateway_id TEXT NOT NULL REFERENCES gateways(id) ON DELETE CASCADE,
                name TEXT NOT NULL,
                platform TEXT NOT NULL,
                public_key TEXT NOT NULL UNIQUE,
                assigned_ip TEXT NOT NULL UNIQUE,
                status TEXT NOT NULL DEFAULT 'PENDING_APPROVAL',
                approved_at INTEGER,
                expires_at INTEGER,
                created_at INTEGER NOT NULL
            );

            CREATE INDEX IF NOT EXISTS idx_devices_gateway ON devices(gateway_id);
            CREATE INDEX IF NOT EXISTS idx_devices_public_key ON devices(public_key);
            CREATE INDEX IF NOT EXISTS idx_devices_status ON devices(status);

            CREATE TABLE IF NOT EXISTS policies (
                id TEXT PRIMARY KEY,
                device_id TEXT NOT NULL UNIQUE REFERENCES devices(id) ON DELETE CASCADE,
                max_down_bps INTEGER NOT NULL DEFAULT 0,
                max_up_bps INTEGER NOT NULL DEFAULT 0,
                daily_quota_bytes INTEGER NOT NULL DEFAULT 0,
                monthly_quota_bytes INTEGER NOT NULL DEFAULT 0,
                allow_lan_access INTEGER NOT NULL DEFAULT 0,
                schedule_json TEXT
            );

            CREATE TABLE IF NOT EXISTS invitations (
                id TEXT PRIMARY KEY,
                gateway_id TEXT NOT NULL REFERENCES gateways(id) ON DELETE CASCADE,
                token_hash TEXT NOT NULL UNIQUE,
                target_platform TEXT NOT NULL,
                pass_type TEXT NOT NULL DEFAULT 'TEMPORARY_24H',
                max_uses INTEGER NOT NULL DEFAULT 1,
                current_uses INTEGER NOT NULL DEFAULT 0,
                expires_at INTEGER NOT NULL,
                is_revoked INTEGER NOT NULL DEFAULT 0,
                created_at INTEGER NOT NULL
            );

            CREATE INDEX IF NOT EXISTS idx_invitations_token_hash ON invitations(token_hash);

            CREATE TABLE IF NOT EXISTS usage_records (
                id TEXT PRIMARY KEY,
                device_id TEXT NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
                timestamp_epoch INTEGER NOT NULL,
                bytes_rx INTEGER NOT NULL DEFAULT 0,
                bytes_tx INTEGER NOT NULL DEFAULT 0,
                duration_seconds INTEGER NOT NULL DEFAULT 0
            );

            CREATE INDEX IF NOT EXISTS idx_usage_device_time ON usage_records(device_id, timestamp_epoch);

            CREATE TABLE IF NOT EXISTS audit_logs (
                id TEXT PRIMARY KEY,
                gateway_id TEXT NOT NULL REFERENCES gateways(id) ON DELETE CASCADE,
                event_type TEXT NOT NULL,
                severity TEXT NOT NULL DEFAULT 'INFO',
                description TEXT NOT NULL,
                metadata_json TEXT,
                created_at INTEGER NOT NULL
            );

            CREATE INDEX IF NOT EXISTS idx_audit_time ON audit_logs(created_at);

            INSERT INTO schema_migrations (version, applied_at) VALUES (1, unixepoch());
        )");

        if (!ok) {
            LOG_ERROR << "Failed to apply migration v1";
            return false;
        }
        tx.Commit();
        LOG_INFO << "Applied schema migration v1 successfully";
    }

    if (current_version < 2) {
        auto tx = BeginTransaction();
        bool ok = Execute(R"(
            CREATE TABLE IF NOT EXISTS billing_settings (
                id TEXT PRIMARY KEY,
                currency TEXT NOT NULL DEFAULT 'INR',
                rate_per_gb REAL NOT NULL DEFAULT 50.0,
                rate_per_hour REAL NOT NULL DEFAULT 20.0,
                updated_at INTEGER NOT NULL
            );

            INSERT OR IGNORE INTO billing_settings (id, currency, rate_per_gb, rate_per_hour, updated_at)
            VALUES ('default', 'INR', 50.0, 20.0, unixepoch());

            CREATE TABLE IF NOT EXISTS call_sessions (
                session_id TEXT PRIMARY KEY,
                dial_pin TEXT NOT NULL,
                device_name TEXT NOT NULL,
                client_ip TEXT NOT NULL,
                platform TEXT NOT NULL,
                start_time INTEGER NOT NULL,
                end_time INTEGER,
                bytes_rx INTEGER NOT NULL DEFAULT 0,
                bytes_tx INTEGER NOT NULL DEFAULT 0,
                duration_seconds INTEGER NOT NULL DEFAULT 0,
                billed_amount REAL NOT NULL DEFAULT 0.0,
                status TEXT NOT NULL DEFAULT 'ACTIVE'
            );

            CREATE INDEX IF NOT EXISTS idx_call_sessions_pin ON call_sessions(dial_pin);
            CREATE INDEX IF NOT EXISTS idx_call_sessions_time ON call_sessions(start_time);

            INSERT INTO schema_migrations (version, applied_at) VALUES (2, unixepoch());
        )");

        if (!ok) {
            LOG_ERROR << "Failed to apply migration v2";
            return false;
        }
        tx.Commit();
        LOG_INFO << "Applied schema migration v2 successfully";
    }

    return true;
}

} // namespace openroute::storage
