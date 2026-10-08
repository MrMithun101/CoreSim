#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace coresim {
struct Transform {
    glm::vec3 position{0.0F};
    glm::quat rotation{1.0F, 0.0F, 0.0F, 0.0F};
    glm::vec3 scale{1.0F};
    // Rotation is a unit quaternion; model order is translation * rotation * scale.
    [[nodiscard]] glm::mat4 matrix() const;
};
} // namespace coresim
