#include <coresim/core/Profiler.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace coresim {
void Profiler::record(ProfileSection section, double milliseconds) noexcept {
    const auto index = static_cast<std::size_t>(section);
    if (!enabled_ || index >= stats_.size() || !std::isfinite(milliseconds) || milliseconds < 0) { return; }
    auto& entry = stats_[index];
    if (entry.count == std::numeric_limits<std::uint64_t>::max() ||
        !std::isfinite(entry.total_ms + milliseconds)) { return; }
    entry.min_ms = entry.count ? std::min(entry.min_ms, milliseconds) : milliseconds;
    entry.max_ms = std::max(entry.max_ms, milliseconds);
    entry.total_ms += milliseconds;
    ++entry.count;
}
void Profiler::reset() noexcept { stats_ = {}; ++generation_; }
void Profiler::set_enabled(bool enabled) noexcept {
    if (enabled_ != enabled) { enabled_ = enabled; ++generation_; }
}
ScopedProfiler::ScopedProfiler(Profiler* profiler, ProfileSection section)
    : profiler_(profiler && profiler->enabled() ? profiler : nullptr), section_(section) {
    if (profiler_) { generation_ = profiler_->generation(); start_ = profiler_->now(); }
}
ScopedProfiler::~ScopedProfiler() {
    if (profiler_ && profiler_->enabled() && generation_ == profiler_->generation()) {
        profiler_->record(section_, std::chrono::duration<double, std::milli>(profiler_->now() - start_).count());
    }
}
} // namespace coresim
