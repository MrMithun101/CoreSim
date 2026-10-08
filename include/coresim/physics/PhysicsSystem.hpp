#pragma once

#include <glm/glm.hpp>

namespace coresim {
class World;
class PhysicsSystem {
public:
    explicit PhysicsSystem(glm::vec3 gravity = {0.0F, -9.81F, 0.0F});
    // Caller supplies fixed ticks. No render state, collision, or angular integration.
    void step(World& world, float delta_seconds) const;
private:
    glm::vec3 gravity_;
};
} // namespace coresim
