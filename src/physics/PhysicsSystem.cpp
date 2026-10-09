#include <coresim/physics/PhysicsSystem.hpp>
#include <coresim/scene/World.hpp>
#include <coresim/core/Profiler.hpp>
#include <coresim/physics/Integration.hpp>
#include <cmath>
#include <stdexcept>

namespace coresim {
namespace {
bool finite(glm::vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
} // namespace
PhysicsSystem::PhysicsSystem(glm::vec3 gravity, BroadPhase mode, float cell_size, BodyIteration iteration)
    : gravity_(gravity), collisions_(mode, cell_size), iteration_(iteration) {
    if (!finite(gravity)) {
        throw std::invalid_argument("Gravity must be finite");
    }
}
void PhysicsSystem::step(World& world, float delta_seconds, CollisionStats* stats, Profiler* profiler) {
    ScopedProfiler timer(profiler, ProfileSection::physics);
    CollisionStats local_stats;
    const bool profiling = profiler && profiler->enabled();
    if (profiling && !stats) { stats = &local_stats; }
    if (!std::isfinite(delta_seconds) || delta_seconds <= 0.0F) {
        throw std::invalid_argument("Physics timestep must be positive and finite");
    }
    integrate_bodies(world, gravity_, delta_seconds, iteration_);
    collisions_.solve(world, stats);
    if (profiling) {
        profiler->record(ProfileSection::broad_phase, stats->broad_phase_ms);
        profiler->record(ProfileSection::narrow_phase, stats->narrow_phase_ms);
        profiler->record(ProfileSection::solver, stats->solver_ms);
    }
}
} // namespace coresim
