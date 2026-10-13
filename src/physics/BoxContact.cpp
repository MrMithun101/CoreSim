#include "BoxContact.hpp"
#include <coresim/physics/AngularDynamics.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace coresim::detail {
namespace {
constexpr float contact_skin = 0.002F; // Keep almost-touching face vertices in the resting patch.
struct Box { glm::vec3 center, half; glm::mat3 axes; };
float radius(const Box& b, glm::vec3 axis) {
    return b.half.x*std::abs(glm::dot(b.axes[0],axis)) + b.half.y*std::abs(glm::dot(b.axes[1],axis)) +
           b.half.z*std::abs(glm::dot(b.axes[2],axis));
}
glm::vec3 support(const Box& b, glm::vec3 direction, int edge = -1) {
    auto p = b.center;
    for (int i = 0; i < 3; ++i) {
        const auto d = glm::dot(b.axes[i],direction);
        if (i != edge && std::abs(d) > 1e-6F) { p += b.axes[i]*b.half[i]*(d > 0 ? 1.0F : -1.0F); }
    }
    return p;
}
struct Polygon { std::array<glm::vec3,16> points{}; std::size_t count{}; };
Polygon clip(const Polygon& input, glm::vec3 normal, glm::vec3 origin, float limit) {
    Polygon result;
    if (input.count == 0) { return result; }
    auto previous = input.points[input.count-1];
    float pd = glm::dot(normal,previous-origin)-limit;
    for (std::size_t i = 0; i < input.count; ++i) {
        const auto point = input.points[i];
        const float d = glm::dot(normal,point-origin)-limit;
        if ((pd <= 0) != (d <= 0)) {
            result.points[result.count++] = previous + (point-previous)*(pd/(pd-d));
        }
        if (d <= 0) { result.points[result.count++] = point; }
        previous = point; pd = d;
    }
    return result;
}
Polygon face(const Box& b, int axis, glm::vec3 direction) {
    const int u = (axis+1)%3, v = (axis+2)%3;
    const auto center = b.center + b.axes[axis]*b.half[axis]*(glm::dot(b.axes[axis],direction) >= 0 ? 1.0F : -1.0F);
    const auto x = b.axes[u]*b.half[u], y = b.axes[v]*b.half[v];
    Polygon result;
    result.points[0] = center-x-y; result.points[1] = center+x-y;
    result.points[2] = center+x+y; result.points[3] = center-x+y;
    result.count = 4;
    return result;
}
glm::vec3 edge_point(const Box& a, const Box& b, int ai, int bi, glm::vec3 normal) {
    const auto ac = support(a,normal,ai), bc = support(b,-normal,bi);
    const auto p = ac-a.axes[ai]*a.half[ai], q = bc-b.axes[bi]*b.half[bi];
    const auto d = a.axes[ai]*(2*a.half[ai]), e = b.axes[bi]*(2*b.half[bi]);
    const auto r = p-q;
    const float aa = glm::dot(d,d), bb = glm::dot(d,e), cc = glm::dot(e,e);
    const float dd = glm::dot(d,r), ee = glm::dot(e,r), denominator = aa*cc-bb*bb;
    float s = denominator > 0 ? std::clamp((bb*ee-cc*dd)/denominator,0.0F,1.0F) : 0;
    float t = (bb*s+ee)/cc;
    if (t < 0) { t = 0; s = std::clamp(-dd/aa,0.0F,1.0F); }
    else if (t > 1) { t = 1; s = std::clamp((bb-dd)/aa,0.0F,1.0F); }
    return (p+d*s+q+e*t)*0.5F;
}
}
ContactManifold box_contacts(const BoxCollider& ca, glm::vec3 pa, glm::quat qa,
                             const BoxCollider& cb, glm::vec3 pb, glm::quat qb) {
    const BoxCollider checked_a(ca.half_extents), checked_b(cb.half_extents);
    const Box a{pa,checked_a.half_extents,glm::mat3_cast(unit_rotation(qa))};
    const Box b{pb,checked_b.half_extents,glm::mat3_cast(unit_rotation(qb))};
    float penetration = std::numeric_limits<float>::max();
    glm::vec3 normal{};
    int feature = -1;
    const auto test = [&](glm::vec3 axis, int index) {
        const float length2 = glm::dot(axis,axis);
        // Parallel edge axes add no separating direction; do not normalize their noise.
        if (length2 < 1e-10F) { return true; }
        axis /= std::sqrt(length2);
        const float distance = glm::dot(pb-pa,axis);
        const float overlap = radius(a,axis)+radius(b,axis)-std::abs(distance);
        if (!std::isfinite(overlap)) { throw std::invalid_argument("Box contact exceeds finite arithmetic range"); }
        if (overlap < 0) { return false; }
        // Keep earlier face axes on equal depths for stable face manifolds.
        if (feature < 0 || (overlap < penetration && (index < 6 || overlap < penetration-1e-5F*(1+penetration)))) { penetration = overlap; normal = distance < 0 ? -axis : axis; feature = index; }
        return true;
    };
    for (int i = 0; i < 3; ++i) { if (!test(a.axes[i],i) || !test(b.axes[i],i+3)) { return {}; } }
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) { if (!test(glm::cross(a.axes[i],b.axes[j]),6+i*3+j)) { return {}; } }
    }
    ContactManifold result;
    if (feature >= 6) {
        result.points[0] = {normal,penetration,edge_point(a,b,(feature-6)/3,(feature-6)%3,normal)};
        result.count = 1; return result;
    }
    const auto& reference = feature < 3 ? a : b;
    const auto& incident = feature < 3 ? b : a;
    const int axis = feature%3;
    const auto outward = feature < 3 ? normal : -normal;
    int incident_axis = 0;
    for (int i = 1; i < 3; ++i) {
        if (std::abs(glm::dot(incident.axes[i],outward)) > std::abs(glm::dot(incident.axes[incident_axis],outward))) { incident_axis = i; }
    }
    auto polygon = face(incident,incident_axis,-outward);
    for (int i = 0; i < 3; ++i) {
        if (i == axis) { continue; }
        polygon = clip(polygon,reference.axes[i],reference.center,reference.half[i]);
        polygon = clip(polygon,-reference.axes[i],reference.center,reference.half[i]);
    }
    const auto face_center = reference.center + outward*reference.half[axis];
    polygon = clip(polygon,outward,face_center,contact_skin);
    for (std::size_t i = 0; i < polygon.count; ++i) {
        const float depth = std::max(0.0F,-glm::dot(outward,polygon.points[i]-face_center));
        const auto point = polygon.points[i]+outward*(depth*0.5F);
        bool duplicate = false;
        for (std::size_t j = 0; j < result.count; ++j) {
            if (glm::length(result.points[j].point-point) < 1e-6F) { duplicate = true; break; }
        }
        if (!duplicate) { result.points[result.count++] = {normal,depth,point}; }
    }
    if (result.count == 0) {
        result.points[0] = {normal,penetration,(support(a,normal)+support(b,-normal))*0.5F};
        result.count = 1;
    }
    return result;
}
ContactManifold box_plane_contacts(const BoxCollider& collider, glm::vec3 position, glm::quat rotation,
                                  const PlaneCollider& plane, glm::vec3 plane_position) {
    const BoxCollider checked(collider.half_extents);
    const Box box{position,checked.half_extents,glm::mat3_cast(unit_rotation(rotation))};
    ContactManifold result;
    for (int vertex = 0; vertex < 8; ++vertex) {
        auto p = position;
        for (int i = 0; i < 3; ++i) { p += box.axes[i]*box.half[i]*((vertex & (1<<i)) ? 1.0F : -1.0F); }
        const float distance = glm::dot(plane.normal,p-plane_position)-plane.offset;
        if (distance <= contact_skin) {
            const float depth = std::max(0.0F,-distance);
            result.points[result.count++] = {-plane.normal,depth,p+plane.normal*(depth*0.5F)};
        }
    }
    return result;
}
} // namespace coresim::detail
