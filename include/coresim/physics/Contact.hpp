#pragma once
#include <coresim/physics/Collider.hpp>
#include <coresim/scene/Entity.hpp>
#include <optional>
#include <array>
#include <glm/gtc/quaternion.hpp>

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
struct ContactManifold {
    std::array<ContactGeometry,16> points{};
    std::size_t count{};
};
[[nodiscard]] ContactManifold detect_manifold(
    const Collider& a, glm::vec3 position_a, const Collider& b, glm::vec3 position_b,
    glm::quat rotation_a = {1,0,0,0}, glm::quat rotation_b = {1,0,0,0});
// Touching counts as contact. Unsupported shape combinations return no contact.
[[nodiscard]] std::optional<ContactGeometry> detect_contact(
    const Collider& a, glm::vec3 position_a, const Collider& b, glm::vec3 position_b,
    glm::quat rotation_a = {1,0,0,0}, glm::quat rotation_b = {1,0,0,0});
} // namespace coresim
