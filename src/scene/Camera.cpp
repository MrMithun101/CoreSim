#include <coresim/scene/Camera.hpp>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace coresim {
Camera::Camera(glm::vec3 position, float yaw, float pitch) : position_(position) {
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
        throw std::invalid_argument("Camera position must be finite");
    }
    look(yaw, pitch);
}
void Camera::look(float yaw_delta, float pitch_delta) {
    if (!std::isfinite(yaw_delta) || !std::isfinite(pitch_delta)) {
        throw std::invalid_argument("Camera look deltas must be finite");
    }
    constexpr double full_turn = 2.0 * std::numbers::pi;
    yaw_ = static_cast<float>(std::remainder(static_cast<double>(yaw_) + yaw_delta, full_turn));
    constexpr float pitch_limit = 1.553343034F; // 89 degrees avoids a singular up vector.
    pitch_ = static_cast<float>(std::clamp(static_cast<double>(pitch_) + pitch_delta,
                                         -static_cast<double>(pitch_limit),
                                         static_cast<double>(pitch_limit)));
}
glm::vec3 Camera::direction() const {
    return {std::cos(yaw_) * std::cos(pitch_), std::sin(pitch_),
            std::sin(yaw_) * std::cos(pitch_)};
}
void Camera::move(glm::vec3 input, float delta_seconds) {
    if (!std::isfinite(delta_seconds) || delta_seconds < 0.0F ||
        !std::isfinite(input.x) || !std::isfinite(input.y) || !std::isfinite(input.z)) {
        throw std::invalid_argument("Camera movement must be finite with a nonnegative timestep");
    }
    const auto forward = direction();
    const auto right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
    auto displacement = right * input.x + glm::vec3(0, 1, 0) * input.y + forward * input.z;
    const float length = glm::length(displacement);
    if (length > 1.0F) {
        displacement /= length;
    }
    position_ += displacement * (8.0F * delta_seconds);
}
glm::mat4 Camera::view() const {
    return glm::lookAt(position_, position_ + direction(), glm::vec3(0, 1, 0));
}
glm::mat4 Camera::projection(float aspect) const {
    if (!std::isfinite(aspect) || aspect <= 0.0F) {
        throw std::invalid_argument("Camera aspect ratio must be positive and finite");
    }
    return glm::perspective(glm::radians(60.0F), aspect, 0.1F, 200.0F);
}
} // namespace coresim
