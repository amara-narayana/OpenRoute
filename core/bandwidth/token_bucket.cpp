#include "core/bandwidth/token_bucket.hpp"
#include <algorithm>

namespace openroute::bandwidth {

TokenBucket::TokenBucket(uint64_t rate_bytes_per_sec, uint64_t max_burst_bytes)
    : rate_bytes_per_sec_(rate_bytes_per_sec),
      max_burst_bytes_(max_burst_bytes),
      current_tokens_(static_cast<double>(max_burst_bytes)),
      last_replenish_time_(std::chrono::steady_clock::now()) {}

void TokenBucket::SetRate(uint64_t rate_bytes_per_sec, uint64_t max_burst_bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    rate_bytes_per_sec_ = rate_bytes_per_sec;
    if (max_burst_bytes == 0) {
        // Default burst depth: 250ms of traffic or min 64KB
        max_burst_bytes_ = std::max<uint64_t>(rate_bytes_per_sec / 4, 65536ULL);
    } else {
        max_burst_bytes_ = max_burst_bytes;
    }
    current_tokens_ = std::min(current_tokens_, static_cast<double>(max_burst_bytes_));
}

void TokenBucket::Replenish() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - last_replenish_time_).count();
    last_replenish_time_ = now;

    if (rate_bytes_per_sec_ > 0 && elapsed > 0) {
        double tokens_to_add = (static_cast<double>(rate_bytes_per_sec_) * elapsed) / 1'000'000.0;
        current_tokens_ = std::min(static_cast<double>(max_burst_bytes_), current_tokens_ + tokens_to_add);
    }
}

bool TokenBucket::Consume(size_t bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (rate_bytes_per_sec_ == 0) {
        return true; // Unlimited
    }

    Replenish();

    double required = static_cast<double>(bytes);
    if (current_tokens_ >= required) {
        current_tokens_ -= required;
        return true;
    }

    return false; // Throttled / packet dropped or delayed
}

void TokenBucket::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    current_tokens_ = static_cast<double>(max_burst_bytes_);
    last_replenish_time_ = std::chrono::steady_clock::now();
}

// ---------------- PeerBandwidthShaper ----------------
PeerBandwidthShaper::PeerBandwidthShaper(uint64_t max_down_bps, uint64_t max_up_bps) {
    SetLimits(max_down_bps, max_up_bps);
}

void PeerBandwidthShaper::SetLimits(uint64_t max_down_bps, uint64_t max_up_bps) {
    // Convert bits per second to bytes per second (bps / 8)
    uint64_t down_Bps = max_down_bps / 8;
    uint64_t up_Bps = max_up_bps / 8;

    download_bucket_.SetRate(down_Bps);
    upload_bucket_.SetRate(up_Bps);
}

bool PeerBandwidthShaper::CheckIngress(size_t bytes) {
    return upload_bucket_.Consume(bytes);
}

bool PeerBandwidthShaper::CheckEgress(size_t bytes) {
    return download_bucket_.Consume(bytes);
}

} // namespace openroute::bandwidth
