#pragma once

#include <glm/glm.hpp>

namespace coresim {
// Position/orientation belong to Transform. Mass zero denotes an immovable body.
class RigidBody {
public:
    explicit RigidBody(float mass = 1.0F, float restitution = 0.5F, float friction = 0.5F);
    void set_mass(float mass);
    void set_restitution(float restitution);
    void set_friction(float friction);
    [[nodiscard]] float friction() const noexcept { return friction_; }
    void add_force(glm::vec3 force);
    void add_torque(glm::vec3 torque);
    // Offset from center of mass and force are both in world coordinates.
    void add_force_at_offset(glm::vec3 force, glm::vec3 offset);
    // Force and torque accumulators both belong to one tick.
    void clear_forces() noexcept { force_ = torque_ = glm::vec3(0); }
    [[nodiscard]] glm::vec3 accumulated_torque() const noexcept { return torque_; }
    [[nodiscard]] float mass() const noexcept { return mass_; }
    [[nodiscard]] float inverse_mass() const noexcept { return inverse_mass_; }
    [[nodiscard]] float restitution() const noexcept { return restitution_; }
    [[nodiscard]] glm::vec3 accumulated_force() const noexcept { return force_; }

    glm::vec3 velocity{0.0F};
    glm::vec3 angular_velocity{0.0F}; // World-space radians per second.
    // Persistent user acceleration, in addition to gravity and force / mass.
    glm::vec3 acceleration{0.0F};
private:
    float mass_{1.0F};
    float inverse_mass_{1.0F};
    float restitution_{0.5F};
    float friction_{0.5F};
    glm::vec3 force_{0.0F};
    glm::vec3 torque_{0.0F};
};
} // namespace coresim
