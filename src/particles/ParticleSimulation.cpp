#include <coresim/particles/ParticleSimulation.hpp>
#include "ParticleKernel.hpp"
#include <coresim/jobs/JobSystem.hpp>
#include <algorithm>
#include <bit>
#include <cmath>
#include <exception>
#include <limits>
#include <stdexcept>
#include <type_traits>
namespace coresim::particles {
static_assert(sizeof(Particle) == 6 * sizeof(float));
static_assert(std::is_standard_layout_v<Particle> && std::is_trivially_copyable_v<Particle>);
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
namespace {
std::array<float, 6> components(const Particle& p) { return {p.x,p.y,p.z,p.vx,p.vy,p.vz}; }

}
std::vector<Particle> initial_particles(std::size_t count, std::uint32_t seed) {
    if (count > maximum_particles) { throw std::invalid_argument("Particle count exceeds 100000"); }
    std::vector<Particle> output(count);
    auto state = seed;
    const auto random = [&]() {
        state = state * 1664525U + 1013904223U;
        return static_cast<float>(state >> 8) * (1.0F / 16777216.0F);
    };
    for (auto& p : output) {
        // Sequenced draws and exact power-of-two scales: positions [-8,8), velocities [-1,1).
        p.x = random() * 16.0F - 8.0F;
        p.y = random() * 16.0F - 8.0F;
        p.z = random() * 16.0F - 8.0F;
        p.vx = random() * 2.0F - 1.0F;
        p.vy = random() * 2.0F - 1.0F;
        p.vz = random() * 2.0F - 1.0F;
    }
    return output;
}
ParticleSimulation::ParticleSimulation(Config config)
    : ParticleSimulation(initial_particles(config.count, config.seed), config.acceleration) {}
ParticleSimulation::ParticleSimulation(std::span<const Particle> initial, Acceleration acceleration)
    : acceleration_(acceleration) {
    detail::validate(initial, acceleration);
    initial_.assign(initial.begin(), initial.end());
    current_ = initial_;
    next_.resize(initial.size());
}
void ParticleSimulation::reset() {
    if (JobSystem::is_worker_thread()) { throw std::logic_error("Particle reset requires a coordinator thread"); }
    std::copy(initial_.begin(), initial_.end(), current_.begin());
    ticks_ = 0;
}
StepStats ParticleSimulation::step_serial() { return advance(nullptr); }
StepStats ParticleSimulation::step_parallel(JobSystem& jobs) { return advance(&jobs); }
StepStats ParticleSimulation::advance(JobSystem* jobs) {
    if (JobSystem::is_worker_thread()) { throw std::logic_error("Particle stepping requires a coordinator thread"); }
    if (ticks_ == std::numeric_limits<std::uint64_t>::max()) { throw std::overflow_error("Particle tick count exhausted"); }
    const auto count = current_.size();
    const auto range = [&](std::size_t begin, std::size_t end) {
        for (auto i = begin; i < end; ++i) { next_[i] = detail::integrate(current_[i], acceleration_); }
    };
    const auto tasks = detail::dispatch(count, jobs, range);
    current_.swap(next_);
    ++ticks_;
    return {count, tasks};
}
Comparison compare(std::span<const Particle> expected, std::span<const Particle> actual) {
    if (expected.size() != actual.size()) { throw std::invalid_argument("Particle comparison counts differ"); }
    Comparison result;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        const auto a = components(expected[i]), b = components(actual[i]);
        bool mismatch = false;
        for (std::size_t component = 0; component < a.size(); ++component) {
            if (!std::isfinite(a[component]) || !std::isfinite(b[component])) {
                mismatch = true;
                result.max_absolute_error = std::numeric_limits<double>::infinity();
            } else {
                const auto error = std::abs(static_cast<double>(a[component]) - b[component]);
                result.max_absolute_error = std::max(result.max_absolute_error, error);
                mismatch = mismatch || a[component] != b[component];
            }
        }
        if (mismatch) { ++result.mismatched_particles; }
    }
    result.equal = result.mismatched_particles == 0;
    return result;
}
std::uint64_t digest(std::span<const Particle> state) noexcept {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const auto& p : state) {
        for (const float value : components(p)) {
            const auto bits = std::bit_cast<std::uint32_t>(value);
            for (unsigned shift = 0; shift < 32; shift += 8) {
                hash ^= (bits >> shift) & 255U;
                hash *= 1099511628211ULL;
            }
        }
    }
    return hash;
}
}
