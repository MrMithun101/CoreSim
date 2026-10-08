#pragma once

#include <glm/glm.hpp>

namespace coresim {
// Position belongs to Transform. Mass zero is an immovable body, not a zero-mass particle.
class RigidBody {
public:
    explicit RigidBody(float mass = 1.0F, float restitution = 0.5F);
    void set_mass(float mass);
    void set_restitution(float restitution);
    void add_force(glm::vec3 force);
    void clear_forces() noexcept { force_ = glm::vec3(0); }
    [[nodiscard]] float mass() const noexcept { return mass_; }
    [[nodiscard]] float inverse_mass() const noexcept { return inverse_mass_; }
    [[nodiscard]] float restitution() const noexcept { return restitution_; }
    [[nodiscard]] glm::vec3 accumulated_force() const noexcept { return force_; }

    glm::vec3 velocity{0.0F};
    // Persistent user acceleration, in addition to gravity and force / mass.
    glm::vec3 acceleration{0.0F};
private:
    float mass_{1.0F};
    float inverse_mass_{1.0F};
    float restitution_{0.5F};
    glm::vec3 force_{0.0F};
};
} // namespace coresim
