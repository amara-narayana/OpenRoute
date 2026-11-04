#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

namespace openroute::tunnel {

constexpr size_t kReplayWindowSize = 128; // 128 packets sliding window

class ReplayWindow {
public:
    ReplayWindow();

    // Returns true if counter is valid (not replayed and within window), false if duplicate or expired
    bool ValidateAndMark(uint64_t counter);

    void Reset();
    [[nodiscard]] uint64_t last_counter() const { return last_counter_; }

private:
    mutable std::mutex mutex_;
    uint64_t last_counter_ = 0;
    bool initialized_ = false;
    uint64_t bitmap_[2] = {0, 0}; // 128 bits
};

} // namespace openroute::tunnel
