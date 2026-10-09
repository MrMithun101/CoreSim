#pragma once
#include <glm/glm.hpp>
#include <variant>

namespace coresim {
// Dimensions are explicit world units. Only Transform::position affects collision geometry.
struct SphereCollider {
    explicit SphereCollider(float radius = 1.0F);
    float radius;
};
struct BoxCollider {
    explicit BoxCollider(glm::vec3 half_extents = glm::vec3(1.0F));
    glm::vec3 half_extents;
};
// Solid half-space below dot(normal, x - transform.position) = offset. Static only.
struct PlaneCollider {
    explicit PlaneCollider(glm::vec3 normal = {0, 1, 0}, float offset = 0.0F);
    glm::vec3 normal;
    float offset;
};
using Collider = std::variant<SphereCollider, BoxCollider, PlaneCollider>;
} // namespace coresim
