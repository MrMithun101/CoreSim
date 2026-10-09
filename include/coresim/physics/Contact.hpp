#pragma once
#include <coresim/physics/Collider.hpp>
#include <coresim/scene/Entity.hpp>
#include <optional>

namespace coresim {
struct ContactGeometry {
    glm::vec3 normal; // From A toward B; correction moves A opposite this direction.
    float penetration;
    glm::vec3 point;
};
struct Contact {
    Entity a;
    Entity b;
    ContactGeometry geometry;
};
// Touching counts as contact. Unsupported shape combinations return no contact.
[[nodiscard]] std::optional<ContactGeometry> detect_contact(
    const Collider& a, glm::vec3 position_a, const Collider& b, glm::vec3 position_b);
} // namespace coresim
