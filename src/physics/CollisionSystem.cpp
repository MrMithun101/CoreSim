#include <coresim/physics/CollisionSystem.hpp>
#include <coresim/physics/AngularDynamics.hpp>
#include <cmath>
#include <stdexcept>
#include <coresim/scene/World.hpp>
#include <algorithm>
#include <array>
#include <chrono>

namespace coresim {
namespace {
float inverse_mass(const World& world, Entity entity) {
    const auto* body = world.rigid_body(entity);
    return body ? body->inverse_mass() : 0.0F;
}
glm::vec3 velocity(const World& world, Entity entity, glm::vec3 offset) {
    const auto* body = world.rigid_body(entity);
    return body && body->inverse_mass() > 0 ? body->velocity + glm::cross(body->angular_velocity,offset) : glm::vec3(0);
}
float friction(const World& world, Entity entity) {
    const auto* body = world.rigid_body(entity);
    return body ? body->friction() : 0.5F;
}
float restitution(const World& world, Entity entity) {
    const auto* body = world.rigid_body(entity);
    return body ? body->restitution() : 1.0F;
}
} // namespace
void CollisionSystem::solve(World& world, CollisionStats* stats) {
    using Clock = std::chrono::steady_clock;
    const auto now = [&] { return stats ? Clock::now() : Clock::time_point{}; };
    const auto milliseconds = [](auto start, auto end) {
        return std::chrono::duration<double, std::milli>(end - start).count();
    };
    if (stats) { *stats = {}; }
    const auto collision_start = now();
    contacts_.clear();
    contact_pairs_.clear();
    const auto colliders = world.colliders();
    // Bounded storage, but still enumerate every unordered pair, without spatial pruning.
    struct Pair { std::size_t a, b; };
    std::array<Pair, 4096> pairs{};
    std::size_t count = 0;
    auto broad_start = now();
    const auto flush = [&] {
        const auto narrow_start = now();
        if (stats) {
            stats->broad_phase_ms += milliseconds(broad_start, narrow_start);
            stats->candidate_pairs += count;
        }
        for (std::size_t index = 0; index < count; ++index) {
            const auto& a = colliders[pairs[index].a];
            const auto& b = colliders[pairs[index].b];
            const auto* ta = world.transform(a.entity);
            const auto* tb = world.transform(b.entity);
            if (!ta || !tb || inverse_mass(world, a.entity) + inverse_mass(world, b.entity) == 0) { continue; }
            if (stats) { ++stats->collision_checks; }
            const auto manifold = detect_manifold(a.value,ta->position,b.value,tb->position,ta->rotation,tb->rotation);
            if (manifold.count == 0) { continue; }
            contact_pairs_.push_back({a.entity,b.entity});
            const auto inverse_tensor = [&](Entity entity, const Collider& shape, glm::quat rotation) {
                const auto* body = world.rigid_body(entity);
                return body && body->inverse_mass() > 0 ? world_tensor(inertia(*body,&shape).inverse_local,rotation) : glm::mat3(0);
            };
            const auto inverse_a = inverse_tensor(a.entity,a.value,ta->rotation);
            const auto inverse_b = inverse_tensor(b.entity,b.value,tb->rotation);
            const float bounce = std::min(restitution(world,a.entity),restitution(world,b.entity));
            for (std::size_t i = 0; i < manifold.count; ++i) {
                const auto& geometry = manifold.points[i];
                const auto ra = geometry.point-ta->position, rb = geometry.point-tb->position;
                const auto cross_a = glm::cross(ra,geometry.normal), cross_b = glm::cross(rb,geometry.normal);
                const auto angular_a = inverse_a*cross_a, angular_b = inverse_b*cross_b;
                const float denominator = inverse_mass(world,a.entity)+inverse_mass(world,b.entity)+
                                          glm::dot(cross_a,angular_a)+glm::dot(cross_b,angular_b);
                const float closing = glm::dot(velocity(world,b.entity,rb)-velocity(world,a.entity,ra),geometry.normal);
                if (!std::isfinite(denominator) || denominator <= 0 || !std::isfinite(closing)) {
                    throw std::runtime_error("Non-finite contact constraint");
                }
                const auto t1 = glm::normalize(glm::cross(geometry.normal,
                    std::abs(geometry.normal.x) < 0.577F ? glm::vec3(1,0,0) : glm::vec3(0,1,0)));
                const auto t2 = glm::cross(geometry.normal,t1);
                const auto effective = [&](glm::vec3 u, glm::vec3 v) {
                    return (inverse_mass(world,a.entity)+inverse_mass(world,b.entity))*glm::dot(u,v)+
                        glm::dot(glm::cross(ra,u),inverse_a*glm::cross(ra,v))+
                        glm::dot(glm::cross(rb,u),inverse_b*glm::cross(rb,v));
                };
                const float k11 = effective(t1,t1), k12 = effective(t1,t2), k22 = effective(t2,t2);
                const glm::mat2 tangent_inverse(k11,k12,k12,k22);
                const float determinant = glm::determinant(tangent_inverse);
                if (!std::isfinite(determinant) || determinant <= 0) { throw std::runtime_error("Invalid tangent effective mass"); }
                contacts_.push_back({{a.entity,b.entity,geometry},closing < -1.0F ? -bounce*closing : 0,0,
                    ra,rb,angular_a,angular_b,denominator,inverse_a,inverse_b,t1,t2,glm::inverse(tangent_inverse),{},
                    std::sqrt(friction(world,a.entity)*friction(world,b.entity))});
            }
        }
        const auto narrow_end = now();
        if (stats) { stats->narrow_phase_ms += milliseconds(narrow_start, narrow_end); }
        count = 0;
        broad_start = now();
    };
    if (mode_ == BroadPhase::spatial_hash) {
        const auto build_start = now();
        spatial_.build(world);
        const auto build_end = now();
        if (stats) { stats->hash_build_ms = milliseconds(build_start, build_end); }
        broad_start = now();
        for (std::size_t i = 0; i < colliders.size(); ++i) {
            for (const auto j : spatial_.query(i)) {
                pairs[count++] = {i, j};
                if (count == pairs.size()) { flush(); }
            }
        }
    } else {
        for (std::size_t i = 0; i < colliders.size(); ++i) {
            for (std::size_t j = i + 1; j < colliders.size(); ++j) {
                pairs[count++] = {i, j};
                if (count == pairs.size()) { flush(); }
            }
        }
    }
    flush();
    if (stats) {
        stats->contacts = contacts_.size();
        stats->query_ms = stats->broad_phase_ms;
        stats->broad_phase_ms += stats->hash_build_ms;
        const auto n = static_cast<std::uint64_t>(colliders.size());
        const auto all_pairs = n > 0 ? n * (n - 1) / 2 : 0;
        stats->pair_reduction_percent = all_pairs == 0 ? 0.0 :
            100.0 * (1.0 - static_cast<double>(stats->candidate_pairs) / static_cast<double>(all_pairs));
    }
    const auto solver_start = now();
    // Sequential impulses accumulate only within this tick; there is no warm starting yet.
    for (int iteration = 0; iteration < 16; ++iteration) {
        for (auto& constraint : contacts_) {
            const auto& contact = constraint.contact;
            const float ia = inverse_mass(world, contact.a);
            const float ib = inverse_mass(world, contact.b);
            const float relative = glm::dot(velocity(world, contact.b, constraint.offset_b) - velocity(world, contact.a, constraint.offset_a),
                                            contact.geometry.normal);
            const float impulse = (constraint.target_speed - relative) / constraint.effective_inverse_mass;
            const float accumulated = std::max(0.0F, constraint.accumulated_impulse + impulse);
            const float magnitude = accumulated - constraint.accumulated_impulse;
            const auto change = magnitude * contact.geometry.normal;
            constraint.accumulated_impulse = accumulated;
            if (ia > 0) {
                auto& body = *world.rigid_body(contact.a);
                body.velocity -= change * ia; body.angular_velocity -= magnitude*constraint.angular_a;
            }
            if (ib > 0) {
                auto& body = *world.rigid_body(contact.b);
                body.velocity += change * ib; body.angular_velocity += magnitude*constraint.angular_b;
            }
            // Solve both tangential directions, then project the accumulated impulse
            // onto the Coulomb disk. This permits resting contact without artificial drag.
            const auto slip = velocity(world,contact.b,constraint.offset_b)-velocity(world,contact.a,constraint.offset_a);
            const glm::vec2 tangential(glm::dot(slip,constraint.tangent_a),glm::dot(slip,constraint.tangent_b));
            auto candidate = constraint.tangent_impulse-constraint.tangent_mass*tangential;
            const float limit = constraint.friction*constraint.accumulated_impulse;
            const float length = glm::length(candidate);
            if (length > limit) { candidate *= limit/length; }
            const auto delta = candidate-constraint.tangent_impulse;
            constraint.tangent_impulse = candidate;
            const auto tangent_change = constraint.tangent_a*delta.x+constraint.tangent_b*delta.y;
            if (ia > 0) {
                auto& body = *world.rigid_body(contact.a);
                body.velocity -= tangent_change*ia;
                body.angular_velocity -= constraint.inverse_a*glm::cross(constraint.offset_a,tangent_change);
            }
            if (ib > 0) {
                auto& body = *world.rigid_body(contact.b);
                body.velocity += tangent_change*ib;
                body.angular_velocity += constraint.inverse_b*glm::cross(constraint.offset_b,tangent_change);
            }
        }
    }
    for (int iteration = 0; iteration < 4; ++iteration) {
        for (const auto& pair : contact_pairs_) {
            const auto a = pair[0], b = pair[1];
            auto& ta = *world.transform(a);
            auto& tb = *world.transform(b);
            if (stats) { ++stats->correction_checks; }
            const auto geometry = detect_contact(*world.collider(a), ta.position,
                                                  *world.collider(b), tb.position, ta.rotation, tb.rotation);
            if (!geometry) { continue; }
            const float ia = inverse_mass(world, a);
            const float ib = inverse_mass(world, b);
            const float correction = 0.8F * std::max(geometry->penetration - 0.001F, 0.0F) / (ia + ib);
            ta.position -= geometry->normal * (correction * ia);
            tb.position += geometry->normal * (correction * ib);
        }
    }
    if (stats) {
        stats->solver_ms = milliseconds(solver_start, now());
        stats->total_collision_ms = milliseconds(collision_start, now());
    }
}
} // namespace coresim
