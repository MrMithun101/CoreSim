#pragma once

#include <glm/glm.hpp>

namespace coresim {
// Free-flight camera, independent of GLFW and rendering resources. Angles are radians.
class Camera {
public:
    explicit Camera(glm::vec3 position = {0.0F, 0.0F, 5.0F},
                    float yaw = -1.570796327F, float pitch = 0.0F);
    void look(float yaw_delta, float pitch_delta);
    // Local axes: right, world-up, forward. Diagonal input is normalized.
    void move(glm::vec3 input, float delta_seconds);
    [[nodiscard]] glm::vec3 position() const noexcept { return position_; }
    [[nodiscard]] glm::vec3 direction() const;
    [[nodiscard]] glm::mat4 view() const;
    [[nodiscard]] glm::mat4 projection(float aspect) const;
private:
    glm::vec3 position_;
    float yaw_{};
    float pitch_{};
};
} // namespace coresim
