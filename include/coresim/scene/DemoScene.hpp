#pragma once

#include <coresim/scene/World.hpp>
#include <coresim/scene/SceneConfig.hpp>
#include <coresim/scene/Camera.hpp>
#include <vector>

namespace coresim {
class DemoScene {
public:
    explicit DemoScene(SceneConfig config = {});
    [[nodiscard]] Camera camera() const;
    [[nodiscard]] glm::vec3 gravity() const noexcept;
    [[nodiscard]] const SceneConfig& config() const noexcept { return config_; }
    void update(float delta_seconds);
    void reset_physics();
    [[nodiscard]] World& world() noexcept { return world_; }
    [[nodiscard]] const World& world() const noexcept { return world_; }
private:
    struct InitialMotion {
        Entity entity;
        glm::vec3 position;
        glm::vec3 velocity;
        glm::quat rotation;
        glm::vec3 angular_velocity;
    };
    void build_stress_scene();
    SceneConfig config_;
    glm::vec3 center_{};
    float radius_{8};
    float particle_extent_{};
    World world_;
    std::vector<InitialMotion> initial_motion_;
};
} // namespace coresim
