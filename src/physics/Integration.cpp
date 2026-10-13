#include <coresim/physics/Integration.hpp>
#include <coresim/physics/AngularDynamics.hpp>
#include <coresim/scene/World.hpp>
#include <cmath>
#include <algorithm>
#include <exception>
#include <coresim/jobs/JobSystem.hpp>
#include <stdexcept>
namespace coresim {
namespace {
bool finite(glm::vec3 value) { return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z); }
}
void integrate_bodies(World& world, glm::vec3 gravity, float delta_seconds, BodyIteration iteration, JobSystem* jobs, std::size_t grain) {
    if (JobSystem::is_worker_thread()) { throw std::logic_error("Integration requires a coordinator thread"); }
    if (grain == 0) { throw std::invalid_argument("Integration grain must be positive"); }
    if (!finite(gravity) || !std::isfinite(delta_seconds) || delta_seconds <= 0) {
        throw std::invalid_argument("Integration requires finite gravity and a positive finite timestep");
    }
    const auto integrate = [&](Entity entity, RigidBody& body) {
        auto* transform = world.transform(entity);
        const auto* collider = world.collider(entity);
        if (collider && std::holds_alternative<PlaneCollider>(*collider) && body.inverse_mass() > 0) {
            throw std::invalid_argument("Plane colliders must be static");
        }
        if (transform && body.inverse_mass() > 0.0F) {
            const auto acceleration = gravity + body.acceleration +
                                      body.accumulated_force() * body.inverse_mass();
            const auto velocity = body.velocity + acceleration * delta_seconds;
            const auto position = transform->position + velocity * delta_seconds;
            if (!finite(velocity) || !finite(position)) {
                throw std::runtime_error("Non-finite rigid body integration result");
            }
            const auto angular = integrate_angular(body, collider, transform->rotation, delta_seconds);
            body.angular_velocity = angular.velocity;
            transform->rotation = angular.rotation;
            // Semi-implicit Euler: the newly integrated velocity advances position.
            body.velocity = velocity;
            transform->position = position;
        }
        // Forces belong to one physics tick, including on static/incomplete entities.
        body.clear_forces();
    };
    const auto range = [&](std::size_t begin, std::size_t end) {
        if (iteration == BodyIteration::dense) {
            world.for_each_rigid_body_range(begin, end, integrate);
        } else {
            const auto entries = world.rigid_bodies();
            for (auto i = begin; i < end; ++i) { integrate(entries[i].entity, *world.rigid_body(entries[i].entity)); }
        }
    };
    const auto count = world.rigid_bodies().size();
    if (!jobs || count < grain) { range(0, count); return; }
    // Bound queue traffic to one task per worker, with at least grain bodies
    // per task. Balanced contiguous ranges avoid a tiny tail and adjacent write sharing.
    const auto tasks = std::min(count / grain, jobs->worker_count());
    const auto width = count / tasks;
    const auto remainder = count % tasks;
    JobGroup group;
    std::size_t begin = 0;
    try {
        for (std::size_t task = 0; task < tasks; ++task) {
            const auto end = begin + width + (task < remainder ? 1 : 0);
            jobs->submit(group, [&, begin, end] { range(begin, end); });
            begin = end;
        }
    } catch (...) {
        // Failed admission can leave earlier jobs borrowing stack/world state.
        // Preserve the admission error, but drain them before unwinding those borrows.
        const auto failure = std::current_exception();
        try { group.wait(); } catch (...) {}
        std::rethrow_exception(failure);
    }
    group.wait();
}
} // namespace coresim
