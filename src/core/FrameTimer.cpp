#include <coresim/core/FrameTimer.hpp>
#include <stdexcept>

namespace coresim {
namespace {
double seconds(FrameTimer::TimePoint from, FrameTimer::TimePoint to) noexcept {
    return std::chrono::duration<double>(to - from).count();
}
} // namespace
FrameTimer::FrameTimer(TimePoint start) noexcept
    : start_(start), frame_start_(start), last_end_(start) {}
void FrameTimer::begin_frame(TimePoint now) {
    if (frame_open_ || now < last_end_) {
        throw std::logic_error("Invalid frame start or non-monotonic timestamp");
    }
    stats_.delta_seconds = seconds(frame_start_, now);
    frame_start_ = now;
    frame_open_ = true;
}
void FrameTimer::end_frame(TimePoint now) {
    if (!frame_open_ || now < frame_start_) {
        throw std::logic_error("Invalid frame end or non-monotonic timestamp");
    }
    stats_.total_frame_seconds = seconds(frame_start_, now);
    stats_.elapsed_seconds = seconds(start_, now);
    ++stats_.frame_count;
    stats_.fps = stats_.elapsed_seconds > 0.0
                     ? static_cast<double>(stats_.frame_count) / stats_.elapsed_seconds
                     : 0.0;
    last_end_ = now;
    frame_open_ = false;
}
} // namespace coresim
