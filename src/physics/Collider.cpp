#include <coresim/physics/Collider.hpp>
#include <cmath>
#include <stdexcept>

namespace coresim {
SphereCollider::SphereCollider(float value) : radius(value) {
    if (!std::isfinite(radius) || radius <= 0) {
        throw std::invalid_argument("Sphere radius must be positive and finite");
    }
}
BoxCollider::BoxCollider(glm::vec3 value) : half_extents(value) {
    for (int i = 0; i < 3; ++i) {
        if (!std::isfinite(value[i]) || value[i] <= 0) {
            throw std::invalid_argument("Box half extents must be positive and finite");
        }
    }
}
PlaneCollider::PlaneCollider(glm::vec3 value, float distance) : normal(value), offset(distance) {
    const float length = glm::length(value);
    if (!std::isfinite(length) || length <= 0 || !std::isfinite(offset)) {
        throw std::invalid_argument("Plane requires a finite nonzero normal and finite offset");
    }
    normal /= length;
}
} // namespace coresim
