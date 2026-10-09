#include <coresim/benchmark/LayoutBenchmark.hpp>
#include <coresim/physics/Integration.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <random>
#include <stdexcept>
namespace coresim {
namespace {
using Clock = std::chrono::steady_clock;
double ms(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end-start).count();
}
void check(glm::vec3 value) {
    if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z)) {
        throw std::runtime_error("Non-finite integration experiment result");
    }
}
}
LayoutTiming LayoutExperiment::step(World& world, IntegrationLayout layout, float dt) {
    if (!std::isfinite(dt) || dt <= 0) { throw std::invalid_argument("Invalid layout timestep"); }
    const auto start = Clock::now();
    if (layout == IntegrationLayout::lookup || layout == IntegrationLayout::dense) {
        integrate_bodies(world, gravity, dt, layout == IntegrationLayout::lookup ? BodyIteration::lookup : BodyIteration::dense);
        const auto end = Clock::now();
        return {0, ms(start,end), 0, ms(start,end)};
    }
    const auto count = world.rigid_bodies().size();
    targets_.resize(count);
    if (layout == IntegrationLayout::aos) { aos_.resize(count); }
    else { for (auto& column : soa_) { column.resize(count); } }
    for (std::size_t i = 0; i < count; ++i) {
        const auto entity = world.rigid_bodies()[i].entity;
        auto& body = *world.rigid_body(entity);
        auto* transform = world.transform(entity);
        const auto* collider = world.collider(entity);
        if (collider && std::holds_alternative<PlaneCollider>(*collider) && body.inverse_mass() > 0) {
            throw std::invalid_argument("Plane colliders must be static");
        }
        targets_[i] = {&body, transform};
        const PackedBody row{transform ? transform->position : glm::vec3(0), body.velocity,
                             body.acceleration, body.accumulated_force(), transform ? body.inverse_mass() : 0};
        if (layout == IntegrationLayout::aos) { aos_[i] = row; }
        else {
            for (int axis = 0; axis < 3; ++axis) {
                const auto a = static_cast<std::size_t>(axis);
                soa_[a][i] = row.position[axis]; soa_[3+a][i] = row.velocity[axis];
                soa_[6+a][i] = row.acceleration[axis]; soa_[9+a][i] = row.force[axis];
            }
            soa_[12][i] = row.inverse_mass;
        }
    }
    const auto gathered = Clock::now();
    for (std::size_t i = 0; i < count; ++i) {
        if (layout == IntegrationLayout::aos) {
            auto& row = aos_[i];
            if (row.inverse_mass == 0) { continue; }
            const auto acceleration = gravity + row.acceleration + row.force * row.inverse_mass;
            const auto velocity = row.velocity + acceleration * dt;
            const auto position = row.position + velocity * dt;
            check(velocity); check(position);
            row.velocity = velocity; row.position = position;
        } else {
            if (soa_[12][i] == 0) { continue; }
            glm::vec3 velocity, position;
            for (int axis = 0; axis < 3; ++axis) {
                const auto a = static_cast<std::size_t>(axis);
                const float acceleration = gravity[axis] + soa_[6+a][i] + soa_[9+a][i] * soa_[12][i];
                velocity[axis] = soa_[3+a][i] + acceleration * dt;
                position[axis] = soa_[a][i] + velocity[axis] * dt;
            }
            check(velocity); check(position);
            for (int axis = 0; axis < 3; ++axis) {
                const auto a = static_cast<std::size_t>(axis);
                soa_[a][i] = position[axis]; soa_[3+a][i] = velocity[axis];
            }
        }
    }
    const auto integrated = Clock::now();
    for (std::size_t i = 0; i < count; ++i) {
        auto& target = targets_[i];
        if (target.transform && target.body->inverse_mass() > 0) {
            if (layout == IntegrationLayout::aos) {
                target.body->velocity = aos_[i].velocity; target.transform->position = aos_[i].position;
            } else {
                for (int axis = 0; axis < 3; ++axis) {
                    const auto a = static_cast<std::size_t>(axis);
                    target.transform->position[axis] = soa_[a][i]; target.body->velocity[axis] = soa_[3+a][i];
                }
            }
        }
        target.body->clear_forces();
    }
    const auto end = Clock::now();
    return {ms(start,gathered), ms(gathered,integrated), ms(integrated,end), ms(start,end)};
}
void make_layout_world(World& world, std::size_t bodies, bool shuffled) {
    std::vector<Entity> entities;
    for (std::size_t i = 0; i < bodies; ++i) {
        const auto e = world.create(); entities.push_back(e);
        auto& body = world.set_rigid_body(e, RigidBody(i % 10 == 0 ? 0.0F : static_cast<float>(1+i%5)));
        body.velocity = {static_cast<float>(i%7)*0.1F, 0.2F, -0.1F};
        body.acceleration = {0.02F, 0.03F, -0.01F};
        body.add_force({0.5F, 1.0F, -0.2F});
        world.set_collider(e, i%2 ? Collider(SphereCollider(0.5F)) : Collider(BoxCollider(glm::vec3(0.5F))));
    }
    std::vector<std::size_t> order(bodies);
    std::iota(order.begin(), order.end(), 0);
    if (shuffled) {
        // Explicit Fisher-Yates avoids implementation-specific std::shuffle distributions.
        std::mt19937 random(104729);
        for (auto i = order.size(); i > 1; --i) { std::swap(order[i-1], order[random() % i]); }
    }
    for (const auto i : order) {
        if (i%17 == 0) { continue; }
        world.set_transform(entities[i]).position = {static_cast<float>(i%100)*3, 20, static_cast<float>(i/100)*3};
    }
}
double layout_checksum(const World& world) {
    double sum = 0;
    for (const auto& entry : world.rigid_bodies()) {
        const auto* transform = world.transform(entry.entity);
        for (int axis = 0; axis < 3; ++axis) {
            sum += static_cast<double>(entry.value.velocity[axis]);
            if (transform) { sum += static_cast<double>(transform->position[axis]); }
        }
    }
    return sum;
}
}
