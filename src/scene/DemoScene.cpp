#include <coresim/scene/DemoScene.hpp>

#include <cmath>
#include <stdexcept>

namespace coresim {
DemoScene::DemoScene() {
    for (std::size_t i = 0; i < transforms_.size(); ++i) {
        auto& transform = transforms_[i];
        const auto column = static_cast<float>(i % 8);
        const auto row = static_cast<float>(i / 8);
        transform.position = {(column - 3.5F) * 3.2F, 0.0F, (row - 3.5F) * 3.2F};
        const float size = 0.5F + static_cast<float>(i % 5) * 0.09F;
        transform.scale = {size, size * (1.0F + static_cast<float>(i % 3) * 0.2F), size};
        transform.rotation = glm::angleAxis(static_cast<float>(i) * 0.37F,
                                            glm::normalize(glm::vec3(0.35F, 1.0F, 0.2F)));
    }
}
void DemoScene::update(float delta_seconds) {
    if (!std::isfinite(delta_seconds) || delta_seconds < 0.0F) {
        throw std::invalid_argument("Scene timestep must be finite and nonnegative");
    }
    for (std::size_t i = 0; i < transforms_.size(); ++i) {
        const auto axis = glm::normalize(glm::vec3(0.2F + static_cast<float>(i % 3), 1.0F, 0.3F));
        const float speed = 0.2F + static_cast<float>(i % 7) * 0.08F;
        const auto delta = glm::angleAxis(speed * delta_seconds, axis);
        transforms_[i].rotation = glm::normalize(delta * transforms_[i].rotation);
    }
}
} // namespace coresim
