#include <coresim/scene/DemoScene.hpp>

#include <cmath>
#include <stdexcept>

namespace coresim {
DemoScene::DemoScene() {
    for (std::size_t i = 0; i < 64; ++i) {
        const auto entity = world_.create();
        Transform transform;
        const auto column = static_cast<float>(i % 8);
        const auto row = static_cast<float>(i / 8);
        transform.position = {(column - 3.5F) * 3.2F, 0.0F, (row - 3.5F) * 3.2F};
        const float size = 0.5F + static_cast<float>(i % 5) * 0.09F;
        transform.scale = {size, size * (1.0F + static_cast<float>(i % 3) * 0.2F), size};
        transform.rotation = glm::angleAxis(static_cast<float>(i) * 0.37F,
                                            glm::normalize(glm::vec3(0.35F, 1.0F, 0.2F)));
        world_.set_transform(entity, transform);
        world_.set_mesh(entity);
        const auto axis = glm::normalize(glm::vec3(0.2F + static_cast<float>(i % 3), 1.0F, 0.3F));
        const float speed = 0.2F + static_cast<float>(i % 7) * 0.08F;
        const bool dynamic = (i % 8 + i / 8) % 2 == 0;
        RigidBody body(dynamic ? 1.0F + static_cast<float>(i % 4) : 0.0F);
        body.velocity = dynamic ? glm::vec3(0, 4.0F + row * 0.2F, 0) : glm::vec3(0);
        world_.set_rigid_body(entity, body);
        initial_motion_[i] = {entity, transform.position, body.velocity};
        if (!dynamic) {
            world_.set_spin(entity, {axis, speed});
        }
    }
}
void DemoScene::reset_physics() {
    for (const auto& initial : initial_motion_) {
        auto* body = world_.rigid_body(initial.entity);
        auto* transform = world_.transform(initial.entity);
        if (body && transform) {
            transform->position = initial.position;
            body->velocity = initial.velocity;
            body->acceleration = glm::vec3(0);
            body->clear_forces();
        }
    }
}
void DemoScene::update(float delta_seconds) {
    if (!std::isfinite(delta_seconds) || delta_seconds < 0.0F) {
        throw std::invalid_argument("Scene timestep must be finite and nonnegative");
    }
    for (const auto& entry : world_.spins()) {
        if (auto* transform = world_.transform(entry.entity)) {
            const auto delta = glm::angleAxis(entry.value.radians_per_second * delta_seconds,
                                              entry.value.axis);
            transform->rotation = glm::normalize(delta * transform->rotation);
        }
    }
}
} // namespace coresim
