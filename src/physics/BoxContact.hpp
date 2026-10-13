#pragma once
#include <coresim/physics/Contact.hpp>
namespace coresim::detail {
ContactManifold box_contacts(const BoxCollider& a, glm::vec3 pa, glm::quat qa,
                             const BoxCollider& b, glm::vec3 pb, glm::quat qb);
ContactManifold box_plane_contacts(const BoxCollider& box, glm::vec3 position, glm::quat rotation,
                                  const PlaneCollider& plane, glm::vec3 plane_position);
}
