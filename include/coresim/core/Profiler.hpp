#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <string_view>

namespace coresim {
enum class ProfileSection : std::size_t {
    frame, physics, broad_phase, narrow_phase, solver, rendering, presentation, count
};
inline constexpr std::array<std::string_view, 7> profile_names{
    "Frame", "Physics", "Broad Phase", "Narrow Phase", "Solver", "Rendering", "Presentation"};
struct ProfileStats {
    std::uint64_t count{};
    double total_ms{}, min_ms{}, max_ms{};
    [[nodiscard]] double average_ms() const noexcept { return count ? total_ms / static_cast<double>(count) : 0; }
};
// Main-thread CPU wall timings, fixed storage, inclusive nested scopes. No GPU timing.
class Profiler {
public:
    using Clock = std::chrono::steady_clock;
    using Now = Clock::time_point (*)();
    explicit Profiler(Now now = &Clock::now) noexcept : now_(now) {}
    void record(ProfileSection section, double milliseconds) noexcept;
    void reset() noexcept;
    void set_enabled(bool enabled) noexcept;
    [[nodiscard]] bool enabled() const noexcept { return enabled_; }
    [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }
    [[nodiscard]] Clock::time_point now() const { return now_(); }
    [[nodiscard]] const auto& stats() const noexcept { return stats_; }
private:
    std::array<ProfileStats, static_cast<std::size_t>(ProfileSection::count)> stats_{};
    Now now_;
    bool enabled_ = true;
    std::uint64_t generation_{};
};
class ScopedProfiler {
public:
    ScopedProfiler(Profiler* profiler, ProfileSection section);
    ~ScopedProfiler();
    ScopedProfiler(const ScopedProfiler&) = delete;
    ScopedProfiler& operator=(const ScopedProfiler&) = delete;
private:
    Profiler* profiler_;
    ProfileSection section_;
    Profiler::Clock::time_point start_{};
    std::uint64_t generation_{};
};
} // namespace coresim
