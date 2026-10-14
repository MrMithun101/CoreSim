#include <coresim/scene/DemoScene.hpp>

#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace coresim {
DemoScene::DemoScene(SceneConfig config) : config_(config) {
    validate_scene_config(config_);
    initial_motion_.reserve(config_.objects);
    if (config_.kind != SceneKind::collision_demo) { build_stress_scene(); return; }
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
        if (!sphere) { transform.rotation = glm::angleAxis(0.12F,glm::normalize(glm::vec3(1,0,1))); }
        world_.set_transform(entity, transform);
        world_.set_mesh(entity, {sphere ? MeshKind::sphere : MeshKind::cube});
        world_.set_collider(entity, sphere ? Collider(SphereCollider(0.65F))
                                           : Collider(BoxCollider(glm::vec3(0.65F))));
        RigidBody body(1.0F + static_cast<float>(i % 4), 0.25F);
        if (!sphere) { body.angular_velocity = {0.35F,0.2F,0.15F}; }
        world_.set_rigid_body(entity, body);
        initial_motion_.push_back({entity, transform.position, body.velocity, transform.rotation, body.angular_velocity});
    }
    // The floor mesh visualizes the plane, which now supports both boxes and spheres.
    const auto floor = world_.create();
    auto& floor_transform = world_.set_transform(floor);
    floor_transform.position = {0, -0.5F, 0};
    floor_transform.scale = {18, 0.5F, 12};
    world_.set_mesh(floor);

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
            transform->rotation = initial.rotation;
            body->angular_velocity = initial.angular_velocity;
            body->acceleration = glm::vec3(0);
            body->clear_forces();
        }
    }
}
void DemoScene::update(float delta_seconds) {
    if (!std::isfinite(delta_seconds) || delta_seconds < 0.0F) {
        throw std::invalid_argument("Scene timestep must be finite and nonnegative");
    }
    if (config_.kind == SceneKind::particle_storm) {
        // A periodic volume keeps this independent-particle workload visible indefinitely.
        // Integration has already joined; this boundary pass is serial and has no collisions.
        for (const auto& initial : initial_motion_) {
            if (auto* transform = world_.transform(initial.entity)) {
                for (int axis=0; axis<3; ++axis) {
                    auto& position=transform->position[axis];
                    position -= 2*particle_extent_*std::floor((position+particle_extent_)/(2*particle_extent_));
                }
            }
        }
    }
    for (const auto& entry : world_.spins()) {
        if (world_.rigid_body(entry.entity)) { continue; } // Physics owns rigid-body orientation.
        if (auto* transform = world_.transform(entry.entity)) {
            const auto delta = glm::angleAxis(entry.value.radians_per_second * delta_seconds,
                                              entry.value.axis);
            transform->rotation = glm::normalize(delta * transform->rotation);
        }
    }
}
glm::vec3 DemoScene::gravity() const noexcept {
    return config_.kind == SceneKind::particle_storm ? glm::vec3(0) : glm::vec3(0,-9.81F,0);
}
Camera DemoScene::camera() const {
    if (config_.kind == SceneKind::collision_demo) { return Camera({16,18,27},-2.106F,-0.52F); }
    const glm::vec3 offset=radius_*glm::vec3(1.35F,0.9F,1.7F);
    const auto direction=-glm::normalize(offset);
    return Camera(center_+offset,std::atan2(direction.z,direction.x),std::asin(direction.y));
}
void DemoScene::build_stress_scene() {
    const bool particles=config_.kind == SceneKind::particle_storm;
    const bool tower=config_.kind == SceneKind::stress_tower;
    std::size_t side=1;
    while (side*side*side < config_.objects) { ++side; }
    const auto layers=(config_.objects+side*side-1)/(side*side);
    const float spacing=particles ? 0.35F : tower ? 1.02F : 1.4F;
    const float half=particles ? 0.045F : tower ? 0.5F : 0.3F;
    const float width=static_cast<float>(side-1)*spacing;
    const float height=static_cast<float>(layers-1)*spacing;
    particle_extent_=(static_cast<float>(side)*spacing)*0.5F+1;
    const float base=tower ? half+0.01F : 3.F+half;
    center_=particles ? glm::vec3(0) : glm::vec3(0,height*0.5F+base,0);
    radius_=std::max(8.F,glm::length(glm::vec3(width+2,height+2,width+2))*0.5F);
    // Explicit 32-bit arithmetic and upper 24 bits avoid library-specific distributions.
    std::uint32_t state=config_.seed;
    const auto random=[&]() {
        state=state*1664525U+1013904223U;
        return static_cast<float>(state>>8)*(1.0F/16777216.0F);
    };
    for (std::size_t i=0;i<config_.objects;++i) {
        const auto e=world_.create();
        Transform t;
        t.position={static_cast<float>(i%side)*spacing-width*0.5F,
                    static_cast<float>(i/(side*side))*spacing+(particles ? -height*0.5F : base),
                    static_cast<float>((i/side)%side)*spacing-width*0.5F};
        t.scale=glm::vec3(half);
        RigidBody body(1,tower ? 0.05F : 0.35F,0.7F);
        if (!tower) {
            const float px=random(), py=random(), pz=random();
            const float vx=random(), vy=random(), vz=random();
            t.position+=glm::vec3(px-0.5F,py-0.5F,pz-0.5F)*0.15F;
            body.velocity=glm::vec3(vx-0.5F,vy-0.5F,vz-0.5F)*(particles ? 4.F : 0.15F);
        } else if (i+1 == config_.objects) {
            t.position.y+=0.3F;
            t.rotation=glm::angleAxis(0.12F,glm::normalize(glm::vec3(1,0,1)));
            body.angular_velocity={0.2F,0.15F+random()*0.1F,0.1F};
        }
        world_.set_transform(e,t);
        world_.set_mesh(e,{tower || particles ? MeshKind::cube : MeshKind::sphere});
        if (!particles) { world_.set_collider(e,tower ? Collider(BoxCollider(glm::vec3(half))) : Collider(SphereCollider(half))); }
        world_.set_rigid_body(e,body);
        initial_motion_.push_back({e,t.position,body.velocity,t.rotation,body.angular_velocity});
    }
    if (!particles) {
        const auto floor=world_.create();
        auto& t=world_.set_transform(floor);
        t.position={0,-0.5F,0}; t.scale={width*0.5F+5,0.5F,width*0.5F+5};
        world_.set_mesh(floor);
        const auto plane=world_.create();
        world_.set_transform(plane); world_.set_collider(plane,PlaneCollider{});
        world_.set_rigid_body(plane,RigidBody(0,1));
    }
}
} // namespace coresim
