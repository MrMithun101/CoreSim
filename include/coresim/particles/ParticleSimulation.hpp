#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
namespace coresim { class JobSystem; }
namespace coresim::particles {
inline constexpr float timestep = 1.0F / 120.0F;
inline constexpr std::size_t maximum_particles = 100000;
// Explicit scalar layout, independent of renderer/ECS/GLM alignment conventions.
struct Particle {
    float x{}, y{}, z{};
    float vx{}, vy{}, vz{};
    bool operator==(const Particle&) const = default;
};
using Acceleration = std::array<float, 3>;
inline constexpr Acceleration default_acceleration{0.0F, -9.81F, 0.0F};
struct Config {
    std::size_t count = 1000;
    std::uint32_t seed = 1;
    Acceleration acceleration = default_acceleration;
};
[[nodiscard]] std::vector<Particle> initial_particles(std::size_t count, std::uint32_t seed);
struct StepStats {
    std::size_t particles{};
    std::size_t worker_tasks{}; // Zero for serial or an empty workload.
};
class ParticleSimulation {
public:
    explicit ParticleSimulation(Config config = {});
    explicit ParticleSimulation(std::span<const Particle> initial,
                                Acceleration acceleration = default_acceleration);
    [[nodiscard]] std::span<const Particle> state() const noexcept { return current_; }
    [[nodiscard]] std::uint64_t ticks() const noexcept { return ticks_; }
    [[nodiscard]] Acceleration acceleration() const noexcept { return acceleration_; }
    void reset();
    [[nodiscard]] StepStats step_serial();
    [[nodiscard]] StepStats step_parallel(JobSystem& jobs);
    // Exclusive coordinator access is required, including reset/read while stepping.
    // Returned spans must not be retained across a step or reset.
    // Workers always join before return/throw; failure preserves current state/ticks.
private:
    StepStats advance(JobSystem* jobs);
    Acceleration acceleration_;
    std::vector<Particle> initial_, current_, next_;
    std::uint64_t ticks_{};
};
struct Comparison {
    bool equal = true;
    std::size_t mismatched_particles{};
    double max_absolute_error{};
};
// Exact, finite comparison for CPU parity. Not a future GPU floating-point tolerance.
[[nodiscard]] Comparison compare(std::span<const Particle> expected, std::span<const Particle> actual);
// Stable scalar-bit digest for replay diagnostics; never used instead of comparison.
[[nodiscard]] std::uint64_t digest(std::span<const Particle> state) noexcept;
}
