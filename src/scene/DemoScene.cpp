#include <coresim/scene/DemoScene.hpp>

#include <cmath>
#include <stdexcept>

namespace coresim {
DemoScene::DemoScene() {
    for (std::size_t i = 0; i < 100; ++i) {
        const auto entity = world_.create();
        const bool sphere = i < 50;
        const auto local = i % 50;
        const auto column = static_cast<float>(local % 5);
        const auto row = static_cast<float>((local / 5) % 5);
        const auto layer = static_cast<float>(local / 25);
        Transform transform;
        transform.position = {(sphere ? -13.0F : 2.0F) + column * 2.7F,
                              2.0F + layer * 2.5F, (row - 2.0F) * 2.7F};
        transform.scale = glm::vec3(0.65F);
        world_.set_transform(entity, transform);
        world_.set_mesh(entity, {sphere ? MeshKind::sphere : MeshKind::cube});
        world_.set_collider(entity, sphere ? Collider(SphereCollider(0.65F))
                                           : Collider(BoxCollider(glm::vec3(0.65F))));
        RigidBody body(1.0F + static_cast<float>(i % 4), 0.25F);
        world_.set_rigid_body(entity, body);
        initial_motion_[i] = {entity, transform.position, body.velocity};
    }
    // Box floor supports boxes. A colocated plane supports spheres; box/plane and
    // sphere/box remain unsupported, so these supports cannot double-resolve a pair.
    const auto floor = world_.create();
    auto& floor_transform = world_.set_transform(floor);
    floor_transform.position = {0, -0.5F, 0};
    floor_transform.scale = {18, 0.5F, 12};
    world_.set_mesh(floor);
    world_.set_collider(floor, BoxCollider(floor_transform.scale));
    world_.set_rigid_body(floor, RigidBody(0, 1));
    const auto plane = world_.create();
    world_.set_transform(plane);
    world_.set_collider(plane, PlaneCollider{});
    world_.set_rigid_body(plane, RigidBody(0, 1));
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
