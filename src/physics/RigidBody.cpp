#include <coresim/physics/RigidBody.hpp>
#include <cmath>
#include <stdexcept>

namespace coresim {
RigidBody::RigidBody(float mass, float restitution) {
    set_mass(mass);
    set_restitution(restitution);
}
void RigidBody::set_mass(float mass) {
    if (!std::isfinite(mass) || mass < 0.0F) {
        throw std::invalid_argument("Mass must be finite and nonnegative");
    }
    const float inverse = mass == 0.0F ? 0.0F : 1.0F / mass;
    if (!std::isfinite(inverse)) {
        throw std::invalid_argument("Mass is too small to represent its inverse");
    }
    mass_ = mass;
    inverse_mass_ = inverse;
}
void RigidBody::set_restitution(float restitution) {
    if (!std::isfinite(restitution) || restitution < 0 || restitution > 1) {
        throw std::invalid_argument("Restitution must be finite and in [0, 1]");
    }
    restitution_ = restitution;
}
void RigidBody::add_force(glm::vec3 force) {
    const auto sum = force_ + force;
    if (!std::isfinite(sum.x) || !std::isfinite(sum.y) || !std::isfinite(sum.z)) {
        throw std::invalid_argument("Accumulated force must remain finite");
    }
    force_ = sum;
}
} // namespace coresim
