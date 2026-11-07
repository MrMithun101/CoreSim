#pragma once
#include <coresim/scene/World.hpp>
#include <coresim/scene/Camera.hpp>
#include <optional>
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
struct Ray { glm::vec3 origin, direction; };
Ray camera_ray(const Camera& camera,glm::vec2 ndc,float aspect);
ObjectSpec primitive_spec(Primitive shape,glm::vec3 position);
class SandboxEditor {
public:
    explicit SandboxEditor(World& world,bool structural=false);
    [[nodiscard]] Entity projectile() const noexcept {return projectile_;}
    bool launch_ready() const {return world_.alive(projectile_) && world_.rigid_body(projectile_)->inverse_mass()>0 && !launched_;}
    glm::vec3 launch_velocity{16,0,0};
    void launch();
    void impulse(glm::vec3 impulse);
    struct Item { Entity entity; ObjectSpec authored; Transform original; };
    const std::vector<Item>& items() const noexcept { return items_; }
    const Item* selected() const;
    void select(Entity id);
    std::optional<float> hit(Ray ray, Entity* nearest=nullptr) const;
    void pick(Ray ray);
    Entity create(ObjectSpec object);
    void change(ObjectSpec object);
    void erase();
    void duplicate();
    void reset_transform();
    void restore();
    bool building() const noexcept { return building_; }
    void simulate() noexcept { building_=false; }
    World& world() { return world_; }
private:
    void require_build() const;
    World& world_;
    std::vector<Item> items_;
    Entity selection_,projectile_;
    bool launched_{};
    bool building_{true};
};
}
