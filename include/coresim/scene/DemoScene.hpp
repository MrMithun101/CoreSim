#pragma once

#include <coresim/scene/World.hpp>
#include <array>

namespace coresim {
class DemoScene {
public:
    DemoScene();
    void update(float delta_seconds);
    void reset_physics();
    [[nodiscard]] World& world() noexcept { return world_; }
    [[nodiscard]] const World& world() const noexcept { return world_; }
private:
    struct InitialMotion {
        Entity entity;
        glm::vec3 position;
        glm::vec3 velocity;
    };
    World world_;
    std::array<InitialMotion, 64> initial_motion_{};
};
} // namespace coresim
