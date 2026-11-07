#pragma once
#include <coresim/scene/Camera.hpp>
#include <coresim/scene/World.hpp>
#include <optional>
#include <variant>
namespace coresim {
enum class Primitive { cube, sphere, platform, ramp, wall, plane };
struct ObjectSpec {
    Primitive primitive{Primitive::cube};
    Finish finish{Finish::steel};
    Transform pose;
    float mass{1}, friction{0.6F}, restitution{0.12F};
    bool gravity{true};
    glm::vec3 velocity{}, angular{};
};
struct Ray {
    glm::vec3 origin, direction;
};
Ray camera_ray(const Camera& camera, glm::vec2 ndc, float aspect);
ObjectSpec primitive_spec(Primitive shape, glm::vec3 position);
class SandboxEditor {
  public:
    explicit SandboxEditor(World& world, bool structural = false);
    [[nodiscard]] Entity projectile() const noexcept { return projectile_; }
    bool launch_ready() const {
        return world_.alive(projectile_) && world_.rigid_body(projectile_)->inverse_mass() > 0 &&
               !launched_;
    }
    glm::vec3 launch_velocity() const noexcept { return launch_velocity_; }
    void set_launch_velocity(glm::vec3 velocity);
    void launch();
    void impulse(glm::vec3 impulse);
    struct Item {
        Entity entity;
        ObjectSpec authored;
        Transform original;
        std::uint64_t key;
    };
    const std::vector<Item>& items() const noexcept { return items_; }
    const Item* selected() const;
    void select(Entity id);
    std::optional<float> hit(Ray ray, Entity* nearest = nullptr) const;
    void pick(Ray ray);
    Entity create(ObjectSpec object);
    void change(ObjectSpec object);
    void erase();
    void duplicate();
    void reset_transform();
    void restore();
    void undo();
    void redo();
    bool can_undo() const { return building_ && !undo_.empty(); }
    bool can_redo() const { return building_ && !redo_.empty(); }
    void begin_gesture() {
        gesture_ = true;
        gesture_changed_ = false;
    }
    void end_gesture() { gesture_ = gesture_changed_ = false; }
    bool building() const noexcept { return building_; }
    void simulate() noexcept { building_ = false; }
    World& world() { return world_; }

  private:
    friend class StudioSession;
    struct ObjectCommand {
        std::uint64_t key, previous_selection;
        std::optional<ObjectSpec> before, after;
        Transform original;
        bool projectile;
    };
    struct LaunchCommand {
        glm::vec3 before, after;
    };
    using Command = std::variant<ObjectCommand, LaunchCommand>;
    glm::vec3 launch_velocity_{16, 0, 0};
    std::vector<Command> undo_, redo_;
    bool gesture_{}, gesture_changed_{};
    std::uint64_t next_key_{1};
    void remember(Command command);
    void replay(const Command& command, bool forward);
    void replace_objects(const std::vector<ObjectSpec>& specs, int projectile, glm::vec3 velocity);
    void require_build() const;
    World& world_;
    std::vector<Item> items_;
    Entity selection_, projectile_;
    bool launched_{};
    bool building_{true};
};
} // namespace coresim
