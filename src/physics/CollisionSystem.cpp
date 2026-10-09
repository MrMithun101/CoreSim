#include <coresim/physics/CollisionSystem.hpp>
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
glm::vec3 velocity(const World& world, Entity entity) {
    const auto* body = world.rigid_body(entity);
    return body && body->inverse_mass() > 0 ? body->velocity : glm::vec3(0);
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
            const auto geometry = detect_contact(a.value, ta->position, b.value, tb->position);
            if (!geometry) { continue; }
            const float closing = glm::dot(velocity(world, b.entity) - velocity(world, a.entity),
                                           geometry->normal);
            const float bounce = std::min(restitution(world, a.entity), restitution(world, b.entity));
            const float target = closing < -1.0F ? -bounce * closing : 0.0F;
            contacts_.push_back({{a.entity, b.entity, *geometry}, target, 0});
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
    for (int iteration = 0; iteration < 8; ++iteration) {
        for (auto& constraint : contacts_) {
            const auto& contact = constraint.contact;
            const float ia = inverse_mass(world, contact.a);
            const float ib = inverse_mass(world, contact.b);
            const float relative = glm::dot(velocity(world, contact.b) - velocity(world, contact.a),
                                            contact.geometry.normal);
            const float impulse = (constraint.target_speed - relative) / (ia + ib);
            const float accumulated = std::max(0.0F, constraint.accumulated_impulse + impulse);
            const auto change = (accumulated - constraint.accumulated_impulse) * contact.geometry.normal;
            constraint.accumulated_impulse = accumulated;
            if (ia > 0) { world.rigid_body(contact.a)->velocity -= change * ia; }
            if (ib > 0) { world.rigid_body(contact.b)->velocity += change * ib; }
        }
    }
    for (int iteration = 0; iteration < 4; ++iteration) {
        for (const auto& constraint : contacts_) {
            const auto& contact = constraint.contact;
            auto& ta = *world.transform(contact.a);
            auto& tb = *world.transform(contact.b);
            if (stats) { ++stats->correction_checks; }
            const auto geometry = detect_contact(*world.collider(contact.a), ta.position,
                                                  *world.collider(contact.b), tb.position);
            if (!geometry) { continue; }
            const float ia = inverse_mass(world, contact.a);
            const float ib = inverse_mass(world, contact.b);
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
