#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace coresim {
struct StepResult {
    unsigned int steps{};
    double dropped_seconds{};
};

class FixedStepper {
public:
    static constexpr double step_seconds = 1.0 / 120.0;
    static constexpr unsigned int max_steps = 16;
    static constexpr double max_frame_seconds = 0.25;

    // Call continuous force producers inside the callback, once per fixed tick.
    template <class Step> StepResult advance(double elapsed_seconds, Step&& step) {
        if (!std::isfinite(elapsed_seconds) || elapsed_seconds < 0.0) {
            throw std::invalid_argument("Frame duration must be finite and nonnegative");
        }
        StepResult result;
        const double accepted = std::min(elapsed_seconds, max_frame_seconds);
        result.dropped_seconds = elapsed_seconds - accepted;
        accumulator_ += accepted;
        // Tolerate roundoff at tick boundaries without admitting a meaningful extra tick.
        constexpr double epsilon = step_seconds * 1e-9;
        while (accumulator_ + epsilon >= step_seconds && result.steps < max_steps) {
            step(static_cast<float>(step_seconds));
            accumulator_ = std::max(0.0, accumulator_ - step_seconds);
            ++result.steps;
        }
        if (accumulator_ + epsilon >= step_seconds) {
            const double ticks = std::floor((accumulator_ + epsilon) / step_seconds);
            const double dropped = ticks * step_seconds;
            accumulator_ = std::max(0.0, accumulator_ - dropped);
            result.dropped_seconds += dropped;
        }
        return result;
    }
    void reset() noexcept { accumulator_ = 0.0; }
    [[nodiscard]] double remainder_seconds() const noexcept { return accumulator_; }
private:
    double accumulator_{};
};
} // namespace coresim
