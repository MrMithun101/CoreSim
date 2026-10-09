#pragma once

#include <glm/glm.hpp>
#include <coresim/physics/CollisionSystem.hpp>

namespace coresim {
class World;
class Profiler;
class PhysicsSystem {
public:
    explicit PhysicsSystem(glm::vec3 gravity = {0.0F, -9.81F, 0.0F},
                           BroadPhase mode = BroadPhase::spatial_hash, float cell_size = 3.0F);
    // Caller supplies fixed ticks. No render state or angular integration.
    void step(World& world, float delta_seconds, CollisionStats* stats = nullptr, Profiler* profiler = nullptr);
private:
    glm::vec3 gravity_;
    CollisionSystem collisions_;
};
} // namespace coresim
