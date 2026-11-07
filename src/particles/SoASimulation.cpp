#include <coresim/particles/SoASimulation.hpp>
#include "ParticleKernel.hpp"
#include <limits>
namespace coresim::particles {
SoASimulation::SoASimulation(Config config)
    : SoASimulation(initial_particles(config.count, config.seed), config.acceleration) {}
SoASimulation::SoASimulation(std::span<const Particle> initial, Acceleration acceleration)
    : acceleration_(acceleration) {
    detail::validate(initial, acceleration);
    for (std::size_t c = 0; c < 6; ++c) {
        initial_[c].resize(initial.size());
        next_[c].resize(initial.size());
    }
    for (std::size_t i = 0; i < initial.size(); ++i) {
        const auto& p = initial[i];
        initial_[0][i]=p.x; initial_[1][i]=p.y; initial_[2][i]=p.z;
        initial_[3][i]=p.vx; initial_[4][i]=p.vy; initial_[5][i]=p.vz;
    }
    current_ = initial_;
}
std::vector<Particle> SoASimulation::snapshot() const {
    std::vector<Particle> result(current_[0].size());
    for (std::size_t i=0; i<result.size(); ++i) {
        result[i]={current_[0][i],current_[1][i],current_[2][i],current_[3][i],current_[4][i],current_[5][i]};
    }
    return result;
}
std::size_t SoASimulation::owned_capacity_bytes() const noexcept {
    std::size_t capacity=0;
    for (std::size_t c=0; c<6; ++c) {
        capacity += initial_[c].capacity()+current_[c].capacity()+next_[c].capacity();
    }
    return capacity*sizeof(float);
}
void SoASimulation::reset() {
    if (JobSystem::is_worker_thread()) { throw std::logic_error("Particle reset requires a coordinator thread"); }
    for (std::size_t c=0; c<6; ++c) { std::copy(initial_[c].begin(),initial_[c].end(),current_[c].begin()); }
    ticks_=0;
}
StepStats SoASimulation::step_serial() { return advance(nullptr); }
StepStats SoASimulation::step_parallel(JobSystem& jobs) { return advance(&jobs); }
StepStats SoASimulation::advance(JobSystem* jobs) {
    if (JobSystem::is_worker_thread()) { throw std::logic_error("Particle stepping requires a coordinator thread"); }
    if (ticks_==std::numeric_limits<std::uint64_t>::max()) { throw std::overflow_error("Particle tick count exhausted"); }
    const auto count=current_[0].size();
    const auto range=[&](std::size_t begin,std::size_t end) {
        for (auto i=begin; i<end; ++i) {
            const Particle p{current_[0][i],current_[1][i],current_[2][i],current_[3][i],current_[4][i],current_[5][i]};
            const auto out=detail::integrate(p,acceleration_);
            next_[0][i]=out.x; next_[1][i]=out.y; next_[2][i]=out.z;
            next_[3][i]=out.vx; next_[4][i]=out.vy; next_[5][i]=out.vz;
        }
    };
    const auto tasks=detail::dispatch(count,jobs,range);
    current_.swap(next_);
    ++ticks_;
    return {count,tasks};
}
}
