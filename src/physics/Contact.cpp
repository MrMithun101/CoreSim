#include <coresim/physics/Contact.hpp>
#include <algorithm>
#include <cmath>

namespace coresim {
namespace {
std::optional<ContactGeometry> spheres(const SphereCollider& a, glm::vec3 pa,
                                       const SphereCollider& b, glm::vec3 pb) {
    const auto delta = pb - pa;
    const float distance = glm::length(delta);
    const float radius = a.radius + b.radius;
    if (distance > radius) { return std::nullopt; }
    // Coincident centers have no unique geometric normal; choose a deterministic axis.
    const auto normal = distance > 0 ? delta / distance : glm::vec3(1, 0, 0);
    return ContactGeometry{normal, radius - distance,
                           0.5F * (pa + normal * a.radius + pb - normal * b.radius)};
}
std::optional<ContactGeometry> boxes(const BoxCollider& a, glm::vec3 pa,
                                     const BoxCollider& b, glm::vec3 pb) {
    const auto delta = pb - pa;
    const auto overlap = a.half_extents + b.half_extents - glm::abs(delta);
    if (overlap.x < 0 || overlap.y < 0 || overlap.z < 0) { return std::nullopt; }
    int axis = 0;
    if (overlap.y < overlap[axis]) { axis = 1; }
    if (overlap.z < overlap[axis]) { axis = 2; }
    glm::vec3 normal(0);
    normal[axis] = delta[axis] < 0 ? -1.0F : 1.0F;
    const auto minimum = glm::max(pa - a.half_extents, pb - b.half_extents);
    const auto maximum = glm::min(pa + a.half_extents, pb + b.half_extents);
    return ContactGeometry{normal, overlap[axis], (minimum + maximum) * 0.5F};
}
std::optional<ContactGeometry> sphere_plane(const SphereCollider& sphere, glm::vec3 ps,
                                            const PlaneCollider& plane, glm::vec3 pp) {
    const float distance = glm::dot(plane.normal, ps - pp) - plane.offset;
    if (distance > sphere.radius) { return std::nullopt; }
    return ContactGeometry{-plane.normal, sphere.radius - distance, ps - plane.normal * distance};
}
} // namespace
std::optional<ContactGeometry> detect_contact(const Collider& a, glm::vec3 pa,
                                              const Collider& b, glm::vec3 pb) {
    if (const auto* sphere = std::get_if<SphereCollider>(&a)) {
        if (const auto* other = std::get_if<SphereCollider>(&b)) { return spheres(*sphere, pa, *other, pb); }
        if (const auto* plane = std::get_if<PlaneCollider>(&b)) { return sphere_plane(*sphere, pa, *plane, pb); }
    } else if (const auto* box = std::get_if<BoxCollider>(&a)) {
        if (const auto* other = std::get_if<BoxCollider>(&b)) { return boxes(*box, pa, *other, pb); }
    } else if (const auto* sphere_b = std::get_if<SphereCollider>(&b)) {
        auto result = sphere_plane(*sphere_b, pb, std::get<PlaneCollider>(a), pa);
        if (result) { result->normal = -result->normal; }
        return result;
    }
    return std::nullopt;
}
} // namespace coresim
