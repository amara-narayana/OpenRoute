#include "core/tunnel/replay_window.hpp"

namespace openroute::tunnel {

ReplayWindow::ReplayWindow() {
    Reset();
}

void ReplayWindow::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    last_counter_ = 0;
    initialized_ = false;
    bitmap_[0] = 0;
    bitmap_[1] = 0;
}

bool ReplayWindow::ValidateAndMark(uint64_t counter) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        initialized_ = true;
        last_counter_ = counter;
        bitmap_[0] = 1; // mark counter
        return true;
    }

    if (counter > last_counter_) {
        uint64_t diff = counter - last_counter_;
        if (diff >= kReplayWindowSize) {
            bitmap_[0] = 0;
            bitmap_[1] = 0;
        } else if (diff >= 64) {
            bitmap_[1] = bitmap_[0] << (diff - 64);
            bitmap_[0] = 0;
        } else {
            bitmap_[1] = (bitmap_[1] << diff) | (bitmap_[0] >> (64 - diff));
            bitmap_[0] = (bitmap_[0] << diff);
        }

        last_counter_ = counter;
        bitmap_[0] |= 1ULL; // mark new highest
        return true;
    }

    // Packet is behind highest seen so far
    uint64_t diff = last_counter_ - counter;
    if (diff >= kReplayWindowSize) {
        return false; // too old
    }

    // Check if bit is set
    size_t word = diff / 64;
    size_t bit = diff % 64;

    if (bitmap_[word] & (1ULL << bit)) {
        return false; // duplicate / replayed
    }

    // Mark bit
    bitmap_[word] |= (1ULL << bit);
    return true;
}

} // namespace openroute::tunnel
