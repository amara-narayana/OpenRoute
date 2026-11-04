#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace openroute::diagnostics {

struct PeerDiagnostics {
    std::string device_id;
    std::string remote_endpoint;
    bool is_direct = true;
    double rtt_ms = 0.0;
    double jitter_ms = 0.0;
    double packet_loss_pct = 0.0;
    uint64_t current_rx_bps = 0; // Download speed
    uint64_t current_tx_bps = 0; // Upload speed
    uint64_t total_rx_bytes = 0;
    uint64_t total_tx_bytes = 0;
    int64_t connection_duration_sec = 0;
    std::string status_summary;
};

struct GatewaySystemMetrics {
    double cpu_usage_pct = 0.0;
    uint64_t memory_used_bytes = 0;
    uint64_t memory_total_bytes = 0;
    double memory_usage_pct = 0.0;
    uint64_t total_egress_bps = 0;
    uint64_t total_ingress_bps = 0;
    bool is_upload_saturated = false;
    std::string human_readable_health;
};

class DiagnosticsEngine {
public:
    DiagnosticsEngine();

    void RecordPing(const std::string& device_id, double rtt_ms);
    void RecordPacketLoss(const std::string& device_id, uint64_t packets_sent, uint64_t packets_lost);
    void UpdateThroughput(const std::string& device_id, uint64_t rx_bytes_delta, uint64_t tx_bytes_delta, double elapsed_sec);

    PeerDiagnostics GetPeerDiagnostics(const std::string& device_id);
    GatewaySystemMetrics SampleSystemMetrics();

    static std::string DiagnoseConnection(const PeerDiagnostics& diag, const GatewaySystemMetrics& sys);

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, PeerDiagnostics> peer_metrics_;

    // CPU sampling state for Windows
#if defined(_WIN32)
    uint64_t last_idle_time_ = 0;
    uint64_t last_kernel_time_ = 0;
    uint64_t last_user_time_ = 0;
#endif
};

} // namespace openroute::diagnostics
