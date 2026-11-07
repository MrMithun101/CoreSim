#pragma once
#include <coresim/particles/ParticleSimulation.hpp>
namespace coresim::particles {
// Same synchronous, transactional contract as ParticleSimulation. Six component streams.
class SoASimulation {
public:
    explicit SoASimulation(Config config = {});
    explicit SoASimulation(std::span<const Particle> initial, Acceleration acceleration = default_acceleration);
    [[nodiscard]] std::vector<Particle> snapshot() const;
    [[nodiscard]] std::uint64_t ticks() const noexcept { return ticks_; }
    [[nodiscard]] std::size_t owned_capacity_bytes() const noexcept;
    void reset();
    [[nodiscard]] StepStats step_serial();
    [[nodiscard]] StepStats step_parallel(JobSystem& jobs);
private:
    using Streams = std::array<std::vector<float>, 6>;
    StepStats advance(JobSystem* jobs);
    Acceleration acceleration_;
    Streams initial_, current_, next_;
    std::uint64_t ticks_{};
};
}
