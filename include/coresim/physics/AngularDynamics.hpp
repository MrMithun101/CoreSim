#pragma once
#include <coresim/physics/Collider.hpp>
#include <coresim/physics/RigidBody.hpp>
#include <glm/gtc/quaternion.hpp>

namespace coresim {
// Principal moments about the center of mass; dimensions ignore render-only scale.
// No collider means an isotropic inertia of mass * identity. Static tensors are zero.
struct Inertia { glm::vec3 local{}, inverse_local{}; };
[[nodiscard]] Inertia inertia(const RigidBody& body, const Collider* collider);
[[nodiscard]] glm::quat unit_rotation(glm::quat rotation);
[[nodiscard]] glm::mat3 world_tensor(glm::vec3 diagonal, glm::quat rotation);
struct AngularStep { glm::quat rotation; glm::vec3 velocity; };
// World-space momentum update, midpoint orientation estimate, normalized quaternion.
[[nodiscard]] AngularStep integrate_angular(const RigidBody& body, const Collider* collider,
                                           glm::quat rotation, float dt);
} // namespace coresim
