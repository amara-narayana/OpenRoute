#pragma once

#include "core/storage/database.hpp"
#include "core/security/crypto.hpp"

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace openroute::device {

enum class DeviceStatus {
    PendingApproval,
    Approved,
    Paused,
    Revoked,
    Expired
};

inline const char* DeviceStatusToString(DeviceStatus status) {
    switch (status) {
        case DeviceStatus::PendingApproval: return "PENDING_APPROVAL";
        case DeviceStatus::Approved:        return "APPROVED";
        case DeviceStatus::Paused:          return "PAUSED";
        case DeviceStatus::Revoked:         return "REVOKED";
        case DeviceStatus::Expired:         return "EXPIRED";
        default:                            return "UNKNOWN";
    }
}

inline DeviceStatus StringToDeviceStatus(std::string_view s) {
    if (s == "APPROVED") return DeviceStatus::Approved;
    if (s == "PAUSED") return DeviceStatus::Paused;
    if (s == "REVOKED") return DeviceStatus::Revoked;
    if (s == "EXPIRED") return DeviceStatus::Expired;
    return DeviceStatus::PendingApproval;
}

struct DeviceRecord {
    std::string id;
    std::string gateway_id;
    std::string name;
    std::string platform; // 'android', 'windows'
    std::string public_key_b64;
    std::string assigned_ip;
    DeviceStatus status = DeviceStatus::PendingApproval;
    int64_t approved_at = 0;
    int64_t expires_at = 0;
    int64_t created_at = 0;
};

class DeviceManager {
public:
    explicit DeviceManager(storage::Database& db);

    // Register a new device via invitation (starts as PENDING_APPROVAL)
    bool RegisterDeviceRequest(const std::string& gateway_id,
                               const std::string& name,
                               const std::string& platform,
                               const std::string& public_key_b64,
                               int64_t expires_at,
                               std::string& out_device_id,
                               std::string& out_assigned_ip);

    bool ApproveDevice(const std::string& device_id);
    bool RejectDevice(const std::string& device_id);
    bool PauseDevice(const std::string& device_id);
    bool ResumeDevice(const std::string& device_id);
    bool RevokeDevice(const std::string& device_id);

    std::optional<DeviceRecord> GetDevice(const std::string& device_id);
    std::optional<DeviceRecord> GetDeviceByPublicKey(const std::string& public_key_b64);
    std::vector<DeviceRecord> ListDevices(const std::string& gateway_id);
    std::vector<DeviceRecord> ListPendingRequests(const std::string& gateway_id);

    // Allocate next available IP in 10.77.0.x pool
    std::string AllocateIp(const std::string& gateway_id);

private:
    storage::Database& db_;
    mutable std::mutex mutex_;
};

} // namespace openroute::device
