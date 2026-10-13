#include <coresim/physics/Contact.hpp>
#include <coresim/physics/AngularDynamics.hpp>
#include "BoxContact.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

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
std::optional<ContactGeometry> sphere_plane(const SphereCollider& sphere, glm::vec3 ps,
                                            const PlaneCollider& plane, glm::vec3 pp) {
    const float distance = glm::dot(plane.normal, ps - pp) - plane.offset;
    if (distance > sphere.radius) { return std::nullopt; }
    return ContactGeometry{-plane.normal, sphere.radius - distance, ps - plane.normal * distance};
}
} // namespace
ContactManifold detect_manifold(const Collider& a, glm::vec3 pa, const Collider& b, glm::vec3 pb,
                                glm::quat qa, glm::quat qb) {
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(pa[axis]) || !std::isfinite(pb[axis])) { throw std::invalid_argument("Contact positions must be finite"); }
    }
    ContactManifold result;
    const auto single = [&](std::optional<ContactGeometry> geometry) {
        if (geometry) { result.points[0] = *geometry; result.count = 1; }
        return result;
    };
    if (const auto* plane = std::get_if<PlaneCollider>(&b)) {
        const PlaneCollider oriented(unit_rotation(qb)*plane->normal,plane->offset);
        if (const auto* sphere = std::get_if<SphereCollider>(&a)) { return single(sphere_plane(*sphere,pa,oriented,pb)); }
        if (const auto* box = std::get_if<BoxCollider>(&a)) { return detail::box_plane_contacts(*box,pa,qa,oriented,pb); }
    } else if (std::holds_alternative<PlaneCollider>(a)) {
        result = detect_manifold(b,pb,a,pa,qb,qa);
        for (std::size_t i = 0; i < result.count; ++i) { result.points[i].normal = -result.points[i].normal; }
        return result;
    } else if (const auto* sphere = std::get_if<SphereCollider>(&a)) {
        if (const auto* other = std::get_if<SphereCollider>(&b)) { return single(spheres(*sphere,pa,*other,pb)); }
    } else if (const auto* box = std::get_if<BoxCollider>(&a)) {
        if (const auto* other = std::get_if<BoxCollider>(&b)) { return detail::box_contacts(*box,pa,qa,*other,pb,qb); }
    }
    return result;
}
std::optional<ContactGeometry> detect_contact(const Collider& a, glm::vec3 pa, const Collider& b, glm::vec3 pb,
                                              glm::quat qa, glm::quat qb) {
    const auto manifold = detect_manifold(a,pa,b,pb,qa,qb);
    if (!manifold.count) { return std::nullopt; }
    auto result = manifold.points[0];
    result.point = glm::vec3(0);
    for (std::size_t i = 0; i < manifold.count; ++i) {
        result.point += manifold.points[i].point;
        result.penetration = std::max(result.penetration,manifold.points[i].penetration);
    }
    result.point /= static_cast<float>(manifold.count);
    return result;
}
} // namespace coresim
