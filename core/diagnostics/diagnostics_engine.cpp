#include "core/diagnostics/diagnostics_engine.hpp"

#if defined(_WIN32)
#include <windows.h>
#endif

#include <cmath>
#include <sstream>

namespace openroute::diagnostics {

namespace {

#if defined(_WIN32)
uint64_t FileTimeToUint64(const FILETIME& ft) {
    return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}
#endif

} // namespace

DiagnosticsEngine::DiagnosticsEngine() {
#if defined(_WIN32)
    FILETIME idle, kernel, user;
    if (GetSystemTimes(&idle, &kernel, &user)) {
        last_idle_time_ = FileTimeToUint64(idle);
        last_kernel_time_ = FileTimeToUint64(kernel);
        last_user_time_ = FileTimeToUint64(user);
    }
#endif
}

void DiagnosticsEngine::RecordPing(const std::string& device_id, double rtt_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& d = peer_metrics_[device_id];
    d.device_id = device_id;

    if (d.rtt_ms > 0.0) {
        // RFC 6298 smoothed jitter calculation
        double diff = std::abs(rtt_ms - d.rtt_ms);
        d.jitter_ms = (d.jitter_ms * 0.75) + (diff * 0.25);
        d.rtt_ms = (d.rtt_ms * 0.8) + (rtt_ms * 0.2);
    } else {
        d.rtt_ms = rtt_ms;
        d.jitter_ms = 0.0;
    }
}

void DiagnosticsEngine::RecordPacketLoss(const std::string& device_id,
                                        uint64_t packets_sent,
                                        uint64_t packets_lost) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& d = peer_metrics_[device_id];
    d.device_id = device_id;
    if (packets_sent > 0) {
        d.packet_loss_pct = (static_cast<double>(packets_lost) / packets_sent) * 100.0;
    } else {
        d.packet_loss_pct = 0.0;
    }
}

void DiagnosticsEngine::UpdateThroughput(const std::string& device_id,
                                         uint64_t rx_bytes_delta,
                                         uint64_t tx_bytes_delta,
                                         double elapsed_sec) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& d = peer_metrics_[device_id];
    d.device_id = device_id;
    d.total_rx_bytes += rx_bytes_delta;
    d.total_tx_bytes += tx_bytes_delta;

    if (elapsed_sec > 0.0) {
        d.current_rx_bps = static_cast<uint64_t>((rx_bytes_delta * 8) / elapsed_sec);
        d.current_tx_bps = static_cast<uint64_t>((tx_bytes_delta * 8) / elapsed_sec);
    }
}

PeerDiagnostics DiagnosticsEngine::GetPeerDiagnostics(const std::string& device_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = peer_metrics_.find(device_id);
    if (it != peer_metrics_.end()) {
        return it->second;
    }
    PeerDiagnostics d{};
    d.device_id = device_id;
    return d;
}

GatewaySystemMetrics DiagnosticsEngine::SampleSystemMetrics() {
    GatewaySystemMetrics m{};

#if defined(_WIN32)
    // 1. CPU Usage
    FILETIME idle, kernel, user;
    if (GetSystemTimes(&idle, &kernel, &user)) {
        uint64_t cur_idle = FileTimeToUint64(idle);
        uint64_t cur_kernel = FileTimeToUint64(kernel);
        uint64_t cur_user = FileTimeToUint64(user);

        uint64_t diff_idle = cur_idle - last_idle_time_;
        uint64_t diff_kernel = cur_kernel - last_kernel_time_;
        uint64_t diff_user = cur_user - last_user_time_;

        uint64_t total_sys = diff_kernel + diff_user;
        if (total_sys > 0) {
            uint64_t total_busy = total_sys - diff_idle;
            m.cpu_usage_pct = (static_cast<double>(total_busy) / total_sys) * 100.0;
        }

        last_idle_time_ = cur_idle;
        last_kernel_time_ = cur_kernel;
        last_user_time_ = cur_user;
    }

    // 2. Memory Usage
    MEMORYSTATUSEX mem_status;
    mem_status.dwLength = sizeof(mem_status);
    if (GlobalMemoryStatusEx(&mem_status)) {
        m.memory_total_bytes = mem_status.ullTotalPhys;
        m.memory_used_bytes = mem_status.ullTotalPhys - mem_status.ullAvailPhys;
        m.memory_usage_pct = static_cast<double>(mem_status.dwMemoryLoad);
    }
#endif

    // Compute aggregate throughput from active peers
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [_, peer] : peer_metrics_) {
        m.total_egress_bps += peer.current_tx_bps;
        m.total_ingress_bps += peer.current_rx_bps;
    }

    // Heuristic: If egress is above 100 Mbps or high CPU, flag saturation
    if (m.total_egress_bps > 95'000'000ULL) {
        m.is_upload_saturated = true;
        m.human_readable_health = "Gateway upload bandwidth is near ISP uplink capacity.";
    } else if (m.cpu_usage_pct > 90.0) {
        m.human_readable_health = "High CPU load detected on gateway host.";
    } else {
        m.human_readable_health = "Gateway operating within healthy parameters.";
    }

    return m;
}

std::string DiagnosticsEngine::DiagnoseConnection(const PeerDiagnostics& diag,
                                                   const GatewaySystemMetrics& sys) {
    if (diag.packet_loss_pct > 5.0) {
        return "High packet loss (" + std::to_string(static_cast<int>(diag.packet_loss_pct)) +
               "%) is affecting connection performance.";
    }
    if (sys.is_upload_saturated) {
        return "Your gateway upload speed is currently saturated by active devices.";
    }
    if (!diag.is_direct) {
        return "This connection is using a relay. Direct connection was unreachable.";
    }
    if (diag.rtt_ms > 200.0) {
        return "High latency (" + std::to_string(static_cast<int>(diag.rtt_ms)) +
               " ms) detected on connection path.";
    }
    return "Connection is direct, stable and performing well.";
}

} // namespace openroute::diagnostics
