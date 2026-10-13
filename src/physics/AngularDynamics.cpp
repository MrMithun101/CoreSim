#include <coresim/physics/AngularDynamics.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace coresim {
namespace {
bool finite(glm::vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
glm::quat advance(glm::quat q, glm::vec3 omega, float dt) {
    const double speed = glm::length(glm::dvec3(omega));
    if (!std::isfinite(speed)) { throw std::runtime_error("Non-finite angular integration result"); }
    if (speed == 0) { return q; }
    const double half_angle = speed * static_cast<double>(dt) * 0.5;
    const auto axis = glm::dvec3(omega) / speed;
    const auto v = axis * std::sin(half_angle);
    return unit_rotation(glm::quat(static_cast<float>(std::cos(half_angle)),
                                  static_cast<float>(v.x),static_cast<float>(v.y),static_cast<float>(v.z)) * q);
}
}
glm::quat unit_rotation(glm::quat q) {
    const double length = glm::length(glm::dquat(q));
    if (!std::isfinite(length) || length == 0) { throw std::invalid_argument("Orientation must be finite and nonzero"); }
    return glm::quat(glm::dquat(q) / length);
}
Inertia inertia(const RigidBody& body, const Collider* collider) {
    if (body.inverse_mass() == 0) { return {}; }
    glm::dvec3 moments(body.mass());
    if (collider) {
        if (const auto* sphere = std::get_if<SphereCollider>(collider)) {
            if (!std::isfinite(sphere->radius) || sphere->radius <= 0) { throw std::invalid_argument("Invalid sphere inertia dimensions"); }
            moments = glm::dvec3(0.4 * body.mass() * static_cast<double>(sphere->radius) * sphere->radius);
        } else if (const auto* box = std::get_if<BoxCollider>(collider)) {
            const auto h = glm::dvec3(box->half_extents);
            if (!finite(box->half_extents) || h.x <= 0 || h.y <= 0 || h.z <= 0) { throw std::invalid_argument("Invalid box inertia dimensions"); }
            moments = static_cast<double>(body.mass()) / 3.0 * glm::dvec3(h.y*h.y+h.z*h.z, h.x*h.x+h.z*h.z, h.x*h.x+h.y*h.y);
        } else { throw std::invalid_argument("Plane colliders must be static"); }
    }
    Inertia result;
    for (int i = 0; i < 3; ++i) {
        const double inverse = 1.0 / moments[i];
        if (!std::isfinite(moments[i]) || moments[i] > std::numeric_limits<float>::max() ||
            !std::isfinite(inverse) || inverse > std::numeric_limits<float>::max()) {
            throw std::invalid_argument("Inertia and its inverse must be representable");
        }
        result.local[i] = static_cast<float>(moments[i]); result.inverse_local[i] = static_cast<float>(inverse);
        if (result.local[i] == 0 || result.inverse_local[i] == 0) { throw std::invalid_argument("Inertia underflow"); }
    }
    return result;
}
glm::mat3 world_tensor(glm::vec3 diagonal, glm::quat rotation) {
    const auto r = glm::mat3_cast(unit_rotation(rotation));
    return glm::mat3(r[0]*diagonal.x, r[1]*diagonal.y, r[2]*diagonal.z) * glm::transpose(r);
}
AngularStep integrate_angular(const RigidBody& body, const Collider* collider, glm::quat rotation, float dt) {
    if (!std::isfinite(dt) || dt <= 0) { throw std::invalid_argument("Angular timestep must be positive and finite"); }
    const auto q = unit_rotation(rotation);
    if (body.inverse_mass() == 0) { return {q,body.angular_velocity}; }
    const auto tensors = inertia(body,collider);
    if (!finite(body.angular_velocity)) { throw std::runtime_error("Angular velocity must be finite"); }
    if (body.angular_velocity == glm::vec3(0) && body.accumulated_torque() == glm::vec3(0)) { return {q,{0,0,0}}; }
    const auto momentum = world_tensor(tensors.local,q) * body.angular_velocity + body.accumulated_torque()*dt;
    // Reconstruct world momentum each tick so direct velocity edits and contact impulses
    // remain authoritative. A midpoint orientation reduces free-top energy drift.
    const auto first = world_tensor(tensors.inverse_local,q)*momentum;
    const auto midpoint = advance(q,first,dt*0.5F);
    const auto next = advance(q,world_tensor(tensors.inverse_local,midpoint)*momentum,dt);
    const auto velocity = world_tensor(tensors.inverse_local,next)*momentum;
    if (!finite(momentum) || !finite(velocity)) { throw std::runtime_error("Non-finite angular integration result"); }
    return {next,velocity};
}
} // namespace coresim
