#include "core/device/device_manager.hpp"
#include "core/common/logger.hpp"

#include <chrono>
#include <sstream>

namespace openroute::device {

DeviceManager::DeviceManager(storage::Database& db) : db_(db) {}

std::string DeviceManager::AllocateIp(const std::string& gateway_id) {
    // Find highest assigned IP in 10.77.0.x
    try {
        auto stmt = db_.Prepare("SELECT assigned_ip FROM devices WHERE gateway_id = ?1 ORDER BY rowid DESC;");
        stmt.BindText(1, gateway_id);

        int max_host = 1; // 10.77.0.1 is gateway
        while (stmt.Step()) {
            std::string ip = stmt.GetText(0);
            auto pos = ip.find_last_of('.');
            if (pos != std::string::npos) {
                int host = std::stoi(ip.substr(pos + 1));
                if (host > max_host) {
                    max_host = host;
                }
            }
        }
        int next_host = max_host + 1;
        if (next_host > 254) {
            next_host = 2; // wrap around or reuse
        }
        return "10.77.0." + std::to_string(next_host);
    } catch (...) {
        return "10.77.0.2";
    }
}

bool DeviceManager::RegisterDeviceRequest(const std::string& gateway_id,
                                          const std::string& name,
                                          const std::string& platform,
                                          const std::string& public_key_b64,
                                          int64_t expires_at,
                                          std::string& out_device_id,
                                          std::string& out_assigned_ip) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if device already registered with this public key
    auto existing = GetDeviceByPublicKey(public_key_b64);
    if (existing.has_value()) {
        out_device_id = existing->id;
        out_assigned_ip = existing->assigned_ip;
        LOG_INFO << "Device with public key " << public_key_b64 << " already registered: " << out_device_id;
        return true;
    }

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    out_device_id = "dev_" + std::to_string(now) + "_" + std::to_string(rand() % 10000);
    out_assigned_ip = AllocateIp(gateway_id);

    try {
        auto stmt = db_.Prepare(R"(
            INSERT INTO devices (id, gateway_id, name, platform, public_key, assigned_ip, status, expires_at, created_at)
            VALUES (?1, ?2, ?3, ?4, ?5, ?6, 'PENDING_APPROVAL', ?7, ?8);
        )");
        stmt.BindText(1, out_device_id);
        stmt.BindText(2, gateway_id);
        stmt.BindText(3, name);
        stmt.BindText(4, platform);
        stmt.BindText(5, public_key_b64);
        stmt.BindText(6, out_assigned_ip);
        stmt.BindInt64(7, expires_at);
        stmt.BindInt64(8, now);
        stmt.Step();

        // Create default policy (unlimited bandwidth & quota, LAN isolation enabled)
        auto pol_stmt = db_.Prepare(R"(
            INSERT INTO policies (id, device_id, max_down_bps, max_up_bps, daily_quota_bytes, monthly_quota_bytes, allow_lan_access)
            VALUES (?1, ?2, 0, 0, 0, 0, 0);
        )");
        pol_stmt.BindText(1, "pol_" + out_device_id);
        pol_stmt.BindText(2, out_device_id);
        pol_stmt.Step();

        LOG_INFO << "New device registered: " << name << " (" << platform << ") id=" << out_device_id;
        return true;
    } catch (const std::exception& ex) {
        LOG_ERROR << "Failed to register device: " << ex.what();
        return false;
    }
}

bool DeviceManager::ApproveDevice(const std::string& device_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        auto stmt = db_.Prepare("UPDATE devices SET status = 'APPROVED', approved_at = ?1 WHERE id = ?2;");
        stmt.BindInt64(1, now);
        stmt.BindText(2, device_id);
        stmt.Step();
        LOG_INFO << "Device approved: " << device_id;
        return true;
    } catch (const std::exception& ex) {
        LOG_ERROR << "Failed to approve device " << device_id << ": " << ex.what();
        return false;
    }
}

bool DeviceManager::RejectDevice(const std::string& device_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        auto stmt = db_.Prepare("DELETE FROM devices WHERE id = ?1;");
        stmt.BindText(1, device_id);
        stmt.Step();
        LOG_INFO << "Device rejected and removed: " << device_id;
        return true;
    } catch (const std::exception& ex) {
        LOG_ERROR << "Failed to reject device " << device_id << ": " << ex.what();
        return false;
    }
}

bool DeviceManager::PauseDevice(const std::string& device_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        auto stmt = db_.Prepare("UPDATE devices SET status = 'PAUSED' WHERE id = ?1;");
        stmt.BindText(1, device_id);
        stmt.Step();
        LOG_INFO << "Device paused: " << device_id;
        return true;
    } catch (const std::exception& ex) {
        LOG_ERROR << "Failed to pause device " << device_id << ": " << ex.what();
        return false;
    }
}

bool DeviceManager::ResumeDevice(const std::string& device_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        auto stmt = db_.Prepare("UPDATE devices SET status = 'APPROVED' WHERE id = ?1;");
        stmt.BindText(1, device_id);
        stmt.Step();
        LOG_INFO << "Device resumed: " << device_id;
        return true;
    } catch (const std::exception& ex) {
        LOG_ERROR << "Failed to resume device " << device_id << ": " << ex.what();
        return false;
    }
}

bool DeviceManager::RevokeDevice(const std::string& device_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        auto stmt = db_.Prepare("UPDATE devices SET status = 'REVOKED' WHERE id = ?1;");
        stmt.BindText(1, device_id);
        stmt.Step();
        LOG_INFO << "Device revoked: " << device_id;
        return true;
    } catch (const std::exception& ex) {
        LOG_ERROR << "Failed to revoke device " << device_id << ": " << ex.what();
        return false;
    }
}

std::optional<DeviceRecord> DeviceManager::GetDevice(const std::string& device_id) {
    try {
        auto stmt = db_.Prepare(R"(
            SELECT id, gateway_id, name, platform, public_key, assigned_ip, status, approved_at, expires_at, created_at
            FROM devices WHERE id = ?1;
        )");
        stmt.BindText(1, device_id);
        if (stmt.Step()) {
            DeviceRecord rec;
            rec.id = stmt.GetText(0);
            rec.gateway_id = stmt.GetText(1);
            rec.name = stmt.GetText(2);
            rec.platform = stmt.GetText(3);
            rec.public_key_b64 = stmt.GetText(4);
            rec.assigned_ip = stmt.GetText(5);
            rec.status = StringToDeviceStatus(stmt.GetText(6));
            rec.approved_at = stmt.GetInt64(7);
            rec.expires_at = stmt.GetInt64(8);
            rec.created_at = stmt.GetInt64(9);
            return rec;
        }
    } catch (...) {}
    return std::nullopt;
}

std::optional<DeviceRecord> DeviceManager::GetDeviceByPublicKey(const std::string& public_key_b64) {
    try {
        auto stmt = db_.Prepare(R"(
            SELECT id, gateway_id, name, platform, public_key, assigned_ip, status, approved_at, expires_at, created_at
            FROM devices WHERE public_key = ?1;
        )");
        stmt.BindText(1, public_key_b64);
        if (stmt.Step()) {
            DeviceRecord rec;
            rec.id = stmt.GetText(0);
            rec.gateway_id = stmt.GetText(1);
            rec.name = stmt.GetText(2);
            rec.platform = stmt.GetText(3);
            rec.public_key_b64 = stmt.GetText(4);
            rec.assigned_ip = stmt.GetText(5);
            rec.status = StringToDeviceStatus(stmt.GetText(6));
            rec.approved_at = stmt.GetInt64(7);
            rec.expires_at = stmt.GetInt64(8);
            rec.created_at = stmt.GetInt64(9);
            return rec;
        }
    } catch (...) {}
    return std::nullopt;
}

std::vector<DeviceRecord> DeviceManager::ListDevices(const std::string& gateway_id) {
    std::vector<DeviceRecord> list;
    try {
        auto stmt = db_.Prepare(R"(
            SELECT id, gateway_id, name, platform, public_key, assigned_ip, status, approved_at, expires_at, created_at
            FROM devices WHERE gateway_id = ?1 ORDER BY created_at DESC;
        )");
        stmt.BindText(1, gateway_id);
        while (stmt.Step()) {
            DeviceRecord rec;
            rec.id = stmt.GetText(0);
            rec.gateway_id = stmt.GetText(1);
            rec.name = stmt.GetText(2);
            rec.platform = stmt.GetText(3);
            rec.public_key_b64 = stmt.GetText(4);
            rec.assigned_ip = stmt.GetText(5);
            rec.status = StringToDeviceStatus(stmt.GetText(6));
            rec.approved_at = stmt.GetInt64(7);
            rec.expires_at = stmt.GetInt64(8);
            rec.created_at = stmt.GetInt64(9);
            list.push_back(rec);
        }
    } catch (...) {}
    return list;
}

std::vector<DeviceRecord> DeviceManager::ListPendingRequests(const std::string& gateway_id) {
    std::vector<DeviceRecord> list;
    try {
        auto stmt = db_.Prepare(R"(
            SELECT id, gateway_id, name, platform, public_key, assigned_ip, status, approved_at, expires_at, created_at
            FROM devices WHERE gateway_id = ?1 AND status = 'PENDING_APPROVAL' ORDER BY created_at ASC;
        )");
        stmt.BindText(1, gateway_id);
        while (stmt.Step()) {
            DeviceRecord rec;
            rec.id = stmt.GetText(0);
            rec.gateway_id = stmt.GetText(1);
            rec.name = stmt.GetText(2);
            rec.platform = stmt.GetText(3);
            rec.public_key_b64 = stmt.GetText(4);
            rec.assigned_ip = stmt.GetText(5);
            rec.status = DeviceStatus::PendingApproval;
            rec.approved_at = stmt.GetInt64(7);
            rec.expires_at = stmt.GetInt64(8);
            rec.created_at = stmt.GetInt64(9);
            list.push_back(rec);
        }
    } catch (...) {}
    return list;
}

} // namespace openroute::device
