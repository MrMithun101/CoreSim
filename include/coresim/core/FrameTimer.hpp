#pragma once
#include <chrono>
#include <cstdint>

namespace coresim {
struct FrameStats {
    double delta_seconds{};
    double total_frame_seconds{};
    double elapsed_seconds{};
    double fps{};
    std::uint64_t frame_count{};
};

// Explicit timestamps let tests exercise clock behavior without sleeps.
class FrameTimer {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    explicit FrameTimer(TimePoint start = Clock::now()) noexcept;
    void begin_frame(TimePoint now);
    void end_frame(TimePoint now);
    [[nodiscard]] const FrameStats& stats() const noexcept { return stats_; }
private:
    TimePoint start_;
    TimePoint frame_start_;
    TimePoint last_end_;
    FrameStats stats_{};
    bool frame_open_{};
};
} // namespace coresim
