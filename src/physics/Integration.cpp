#include <coresim/physics/Integration.hpp>
#include <coresim/scene/World.hpp>
#include <cmath>
#include <stdexcept>
namespace coresim {
namespace {
bool finite(glm::vec3 value) { return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z); }
}
void integrate_bodies(World& world, glm::vec3 gravity, float delta_seconds, BodyIteration iteration) {
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
            // Semi-implicit Euler: the newly integrated velocity advances position.
            body.velocity = velocity;
            transform->position = position;
        }
        // Forces belong to one physics tick, including on static/incomplete entities.
        body.clear_forces();
    };
    if (iteration == BodyIteration::dense) {
        world.for_each_rigid_body(integrate);
    } else {
        for (const auto& entry : world.rigid_bodies()) { integrate(entry.entity, *world.rigid_body(entry.entity)); }
    }
}
} // namespace coresim
