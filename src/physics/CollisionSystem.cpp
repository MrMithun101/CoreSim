#include <coresim/physics/CollisionSystem.hpp>
#include <coresim/scene/World.hpp>
#include <algorithm>

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
void CollisionSystem::solve(World& world) {
    contacts_.clear();
    const auto colliders = world.colliders();
    // Deliberately straightforward all-pairs baseline; spatial acceleration comes later.
    for (std::size_t i = 0; i < colliders.size(); ++i) {
        const auto& a = colliders[i];
        const auto* ta = world.transform(a.entity);
        if (!ta) { continue; }
        for (std::size_t j = i + 1; j < colliders.size(); ++j) {
            const auto& b = colliders[j];
            const auto* tb = world.transform(b.entity);
            if (!tb || inverse_mass(world, a.entity) + inverse_mass(world, b.entity) == 0) { continue; }
            const auto geometry = detect_contact(a.value, ta->position, b.value, tb->position);
            if (!geometry) { continue; }
            const float closing = glm::dot(velocity(world, b.entity) - velocity(world, a.entity),
                                           geometry->normal);
            const float bounce = std::min(restitution(world, a.entity), restitution(world, b.entity));
            // Suppress tiny repeated restitution bounces at resting contacts.
            const float target = closing < -1.0F ? -bounce * closing : 0.0F;
            contacts_.push_back({{a.entity, b.entity, *geometry}, target, 0});
        }
    }
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
}
} // namespace coresim
