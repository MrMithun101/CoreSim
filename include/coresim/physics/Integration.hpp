#pragma once
#include <glm/glm.hpp>
namespace coresim {
class World;
enum class BodyIteration { lookup, dense };
// Integrate and clear per-tick forces; no collision solve. Retains the lookup reference.
void integrate_bodies(World& world, glm::vec3 gravity, float delta_seconds,
                      BodyIteration iteration = BodyIteration::lookup);
}
