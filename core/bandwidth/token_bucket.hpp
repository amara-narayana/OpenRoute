#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>

namespace openroute::bandwidth {

class TokenBucket {
public:
    // rate_bytes_per_sec: 0 = unlimited
    // max_burst_bytes: maximum token depth
    explicit TokenBucket(uint64_t rate_bytes_per_sec = 0, uint64_t max_burst_bytes = 65536);

    void SetRate(uint64_t rate_bytes_per_sec, uint64_t max_burst_bytes = 0);
    [[nodiscard]] uint64_t rate_bytes_per_sec() const { return rate_bytes_per_sec_; }

    // Consumes tokens for a packet of given size.
    // Returns true if allowed, false if rate limit exceeded.
    bool Consume(size_t bytes);

    void Reset();

private:
    void Replenish();

    mutable std::mutex mutex_;
    uint64_t rate_bytes_per_sec_ = 0;
    uint64_t max_burst_bytes_ = 65536;
    double current_tokens_ = 65536.0;
    std::chrono::steady_clock::time_point last_replenish_time_;
};

class PeerBandwidthShaper {
public:
    PeerBandwidthShaper(uint64_t max_down_bps = 0, uint64_t max_up_bps = 0);

    void SetLimits(uint64_t max_down_bps, uint64_t max_up_bps);

    // Ingress (Client -> Gateway -> WAN): upload from client's perspective
    bool CheckIngress(size_t bytes);

    // Egress (WAN -> Gateway -> Client): download from client's perspective
    bool CheckEgress(size_t bytes);

private:
    TokenBucket upload_bucket_;   // bytes/sec
    TokenBucket download_bucket_; // bytes/sec
};

} // namespace openroute::bandwidth
