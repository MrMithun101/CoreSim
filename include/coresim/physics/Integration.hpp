#pragma once
#include <glm/glm.hpp>
#include <cstddef>
namespace coresim {
class World;
class JobSystem;
inline constexpr std::size_t default_integration_grain = 16384;
enum class BodyIteration { lookup, dense };
// Synchronous integration: no structural edits or concurrent access to written values.
// Optional workers borrow disjoint body ranges; all jobs finish even on failure.
// Below grain, run inline. Worker-thread callers and zero grain are rejected.
void integrate_bodies(World& world, glm::vec3 gravity, float delta_seconds,
                      BodyIteration iteration = BodyIteration::lookup,
                      JobSystem* jobs = nullptr, std::size_t grain = default_integration_grain);
}
