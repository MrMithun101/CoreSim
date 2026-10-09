#include <coresim/physics/PhysicsSystem.hpp>
#include <coresim/scene/World.hpp>
#include <coresim/core/Profiler.hpp>
#include <cmath>
#include <stdexcept>

namespace coresim {
namespace {
bool finite(glm::vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
} // namespace
PhysicsSystem::PhysicsSystem(glm::vec3 gravity, BroadPhase mode, float cell_size)
    : gravity_(gravity), collisions_(mode, cell_size) {
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
    for (const auto& entry : world.rigid_bodies()) {
        auto& body = *world.rigid_body(entry.entity);
        auto* transform = world.transform(entry.entity);
        const auto* collider = world.collider(entry.entity);
        if (collider && std::holds_alternative<PlaneCollider>(*collider) && body.inverse_mass() > 0) {
            throw std::invalid_argument("Plane colliders must be static");
        }
        if (transform && body.inverse_mass() > 0.0F) {
            const auto acceleration = gravity_ + body.acceleration +
                                      body.accumulated_force() * body.inverse_mass();
            const auto velocity = body.velocity + acceleration * delta_seconds;
            const auto position = transform->position + velocity * delta_seconds;
            if (!finite(velocity) || !finite(position)) {
                throw std::runtime_error("Non-finite rigid body integration result");
            }
            // Semi-implicit Euler: the newly integrated velocity advances position.
            body.velocity = velocity;
            transform->position = position;
        }
        // Forces belong to one physics tick, including on static/incomplete entities.
        body.clear_forces();
    }
    collisions_.solve(world, stats);
    if (profiling) {
        profiler->record(ProfileSection::broad_phase, stats->broad_phase_ms);
        profiler->record(ProfileSection::narrow_phase, stats->narrow_phase_ms);
        profiler->record(ProfileSection::solver, stats->solver_ms);
    }
}
} // namespace coresim
