#include <coresim/physics/PhysicsSystem.hpp>
#include <coresim/core/FixedStepper.hpp>
#include <coresim/scene/DemoScene.hpp>
#include <cmath>
#include <coresim/physics/Contact.hpp>
#include <coresim/scene/World.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

using namespace coresim;
TEST_CASE("Sphere contacts cover separation touching overlap coincidence and reversal") {
    const Collider sphere = SphereCollider(1);
    REQUIRE_FALSE(detect_contact(sphere, {0,0,0}, sphere, {2.1F,0,0}));
    const auto touching = detect_contact(sphere, {0,0,0}, sphere, {2,0,0});
    REQUIRE(touching);
    REQUIRE(touching->penetration == 0);
    const auto overlap = detect_contact(sphere, {0,0,0}, sphere, {1.5F,0,0});
    REQUIRE(overlap);
    REQUIRE(overlap->normal == glm::vec3(1,0,0));
    REQUIRE(overlap->penetration == Catch::Approx(0.5F));
    REQUIRE(overlap->point.x == Catch::Approx(0.75F));
    const auto reverse = detect_contact(sphere, {1.5F,0,0}, sphere, {0,0,0});
    REQUIRE(reverse->normal == -overlap->normal);
    const auto coincident = detect_contact(sphere, {0,0,0}, sphere, {0,0,0});
    REQUIRE(coincident->normal == glm::vec3(1,0,0));
    REQUIRE(coincident->penetration == 2);
}
TEST_CASE("AABB contacts choose minimum translation including containment") {
    const Collider box = BoxCollider(glm::vec3(1));
    REQUIRE_FALSE(detect_contact(box, {0,0,0}, box, {0,0,2.1F}));
    const auto face = detect_contact(box, {0,0,0}, box, {0,2,0});
    REQUIRE(face);
    REQUIRE(face->penetration == 0);
    const auto overlap = detect_contact(box, {0,0,0}, box, {0,1.75F,0.1F});
    REQUIRE(overlap->normal == glm::vec3(0,1,0));
    REQUIRE(overlap->penetration == Catch::Approx(0.25F));
    const auto contained = detect_contact(BoxCollider(glm::vec3(3)), {0,0,0}, box, {1,0,0});
    REQUIRE(contained->penetration == Catch::Approx(3));
    REQUIRE(contained->normal == glm::vec3(1,0,0));
    const auto reverse = detect_contact(box, {0,1.75F,0.1F}, box, {0,0,0});
    REQUIRE(reverse->normal == -overlap->normal);
}
TEST_CASE("Sphere plane contacts use a translated one-sided solid half-space") {
    const Collider sphere = SphereCollider(1);
    const Collider plane = PlaneCollider({0,2,0}, 1);
    REQUIRE_FALSE(detect_contact(sphere, {0,4.1F,0}, plane, {0,2,0}));
    const auto contact = detect_contact(sphere, {0,3.5F,0}, plane, {0,2,0});
    REQUIRE(contact);
    REQUIRE(contact->normal == glm::vec3(0,-1,0));
    REQUIRE(contact->penetration == Catch::Approx(0.5F));
    REQUIRE(contact->point.y == Catch::Approx(3));
    const auto below = detect_contact(sphere, {0,1,0}, plane, {0,2,0});
    REQUIRE(below->penetration == Catch::Approx(3));
    const auto reverse = detect_contact(plane, {0,2,0}, sphere, {0,3.5F,0});
    REQUIRE(reverse->normal == -contact->normal);
    REQUIRE_FALSE(detect_contact(sphere, {0,0,0}, BoxCollider{}, {0,0,0}));
    REQUIRE_FALSE(detect_contact(plane, {0,0,0}, plane, {0,0,0}));
}
TEST_CASE("Collider shape validation and component lifecycle") {
    REQUIRE_THROWS_AS(SphereCollider(0), std::invalid_argument);
    REQUIRE_THROWS_AS(BoxCollider(glm::vec3(-1)), std::invalid_argument);
    REQUIRE_THROWS_AS(PlaneCollider(glm::vec3(0)), std::invalid_argument);
    World world;
    const auto entity = world.create();
    world.set_collider(entity, SphereCollider{});
    REQUIRE(std::holds_alternative<SphereCollider>(*world.collider(entity)));
    world.set_collider(entity, BoxCollider{});
    REQUIRE(world.colliders().size() == 1);
    REQUIRE(world.remove_collider(entity));
    REQUIRE_FALSE(world.remove_collider(entity));
    world.set_collider(entity, PlaneCollider{});
    REQUIRE(world.destroy(entity));
    const auto replacement = world.create();
    REQUIRE(world.collider(entity) == nullptr);
    REQUIRE(world.collider(replacement) == nullptr);
    REQUIRE_THROWS_AS(world.set_collider(entity, SphereCollider{}), std::invalid_argument);
}


namespace {
Entity body(World& world, Collider collider, glm::vec3 position, float mass = 1, float bounce = 0) {
    const auto entity = world.create();
    world.set_transform(entity).position = position;
    world.set_collider(entity, collider);
    world.set_rigid_body(entity, RigidBody(mass, bounce));
    return entity;
}
}
TEST_CASE("Equal-mass sphere impulse preserves momentum and follows restitution") {
    for (const float bounce : {0.0F, 0.5F, 1.0F}) {
        World world;
        const auto a = body(world, SphereCollider(1), {-1,0,0}, 1, bounce);
        const auto b = body(world, SphereCollider(1), {1,0,0}, 1, bounce);
        world.rigid_body(a)->velocity.x = 2;
        world.rigid_body(b)->velocity.x = -2;
        PhysicsSystem physics({0,0,0});
        physics.step(world, 0.01F);
        REQUIRE(world.rigid_body(a)->velocity.x == Catch::Approx(-2 * bounce).margin(0.00001));
        REQUIRE(world.rigid_body(b)->velocity.x == Catch::Approx(2 * bounce).margin(0.00001));
        REQUIRE(world.rigid_body(a)->velocity.x + world.rigid_body(b)->velocity.x == Catch::Approx(0).margin(0.00001));
    }
}
TEST_CASE("Separating contacts are not pulled together and static bodies remain immovable") {
    World world;
    const auto a = body(world, BoxCollider{}, {0,0,0});
    const auto b = body(world, BoxCollider{}, {1.5F,0,0}, 0);
    world.rigid_body(a)->velocity = {-2,0,0};
    world.rigid_body(b)->velocity = {-100,0,0}; // Ignored for a static body.
    PhysicsSystem physics({0,0,0});
    physics.step(world, 0.01F);
    REQUIRE(world.rigid_body(a)->velocity.x == -2);
    REQUIRE(world.transform(b)->position == glm::vec3(1.5F,0,0));
    REQUIRE(world.transform(a)->position.x < -0.49F);
    world.rigid_body(a)->set_mass(0);
    REQUIRE_NOTHROW(physics.step(world, 0.01F));
}
TEST_CASE("Sphere floor bounce uses restitution and resting spheres settle without sinking") {
    World world;
    body(world, PlaneCollider{}, {0,0,0}, 0, 1);
    const auto sphere = body(world, SphereCollider(1), {0,1,0}, 1, 0.5F);
    world.rigid_body(sphere)->velocity.y = -4;
    PhysicsSystem no_gravity({0,0,0});
    no_gravity.step(world, 0.01F);
    REQUIRE(world.rigid_body(sphere)->velocity.y == Catch::Approx(2));
    PhysicsSystem gravity;
    for (int i = 0; i < 1200; ++i) { gravity.step(world, static_cast<float>(FixedStepper::step_seconds)); }
    REQUIRE(world.transform(sphere)->position.y >= 0.998F);
    REQUIRE(world.transform(sphere)->position.y < 1.01F);
    REQUIRE(std::abs(world.rigid_body(sphere)->velocity.y) < 0.01F);
}
TEST_CASE("Inverse mass weights separation and missing-body colliders are static") {
    World world;
    const auto a = body(world, SphereCollider(1), {0,0,0}, 1);
    const auto b = body(world, SphereCollider(1), {1,0,0}, 3);
    PhysicsSystem physics({0,0,0});
    physics.step(world, 0.01F);
    const float moved_a = -world.transform(a)->position.x;
    const float moved_b = world.transform(b)->position.x - 1;
    REQUIRE(moved_a == Catch::Approx(3 * moved_b));
    REQUIRE(world.remove_rigid_body(b));
    const auto before = world.transform(b)->position;
    physics.step(world, 0.01F);
    REQUIRE(world.transform(b)->position == before);
    const auto invalid = body(world, PlaneCollider{}, {0,-5,0}, 1);
    REQUIRE_THROWS_AS(physics.step(world, 0.01F), std::invalid_argument);
    REQUIRE(world.destroy(invalid));
}
TEST_CASE("One hundred AABB bodies remain finite and supported on a floor") {
    World world;
    body(world, BoxCollider({20,0.5F,20}), {0,-0.5F,0}, 0);
    std::vector<Entity> cubes;
    for (int i = 0; i < 100; ++i) {
        cubes.push_back(body(world, BoxCollider(glm::vec3(0.5F)),
            {static_cast<float>(i % 10) * 2 - 9, 2 + static_cast<float>(i % 3),
             static_cast<float>(i / 10) * 2 - 9}));
    }
    PhysicsSystem physics;
    for (int tick = 0; tick < 600; ++tick) { physics.step(world, static_cast<float>(FixedStepper::step_seconds)); }
    for (const auto entity : cubes) {
        REQUIRE(world.transform(entity)->position.y > 0.498F);
        REQUIRE(world.transform(entity)->position.y < 0.51F);
        REQUIRE(std::abs(world.rigid_body(entity)->velocity.y) < 0.01F);
    }
}

TEST_CASE("Mixed demo supports one hundred bodies and two-body stacks over ten seconds") {
    DemoScene scene;
    PhysicsSystem physics;
    for (int tick = 0; tick < 1200; ++tick) {
        physics.step(scene.world(), static_cast<float>(FixedStepper::step_seconds));
    }
    for (const auto& entry : scene.world().rigid_bodies()) {
        if (entry.value.inverse_mass() == 0) { continue; }
        const float height = scene.world().transform(entry.entity)->position.y;
        REQUIRE(height > 0.62F);
        REQUIRE(height < 2.0F);
        REQUIRE(glm::length(entry.value.velocity) < 0.05F);
    }
    scene.reset_physics();
    REQUIRE(scene.world().transform(scene.world().rigid_bodies()[0].entity)->position.y == 2.0F);
}

TEST_CASE("Naive metrics enumerate all pairs across batch boundaries and reset each tick") {
    World world;
    CollisionSystem collisions(BroadPhase::naive);
    CollisionStats stats;
    collisions.solve(world, &stats);
    REQUIRE(stats.candidate_pairs == 0);
    for (int i = 0; i < 93; ++i) {
        const auto entity = world.create();
        world.set_transform(entity).position = {static_cast<float>(i) * 3, 0, 0};
        world.set_collider(entity, SphereCollider(1));
        world.set_rigid_body(entity);
    }
    for (int tick = 0; tick < 2; ++tick) {
        collisions.solve(world, &stats);
        REQUIRE(stats.candidate_pairs == 4278);
        REQUIRE(stats.collision_checks == 4278);
        REQUIRE(stats.contacts == 0);
        REQUIRE(stats.correction_checks == 0);
        REQUIRE(std::isfinite(stats.broad_phase_ms));
        REQUIRE(stats.broad_phase_ms >= 0);
        REQUIRE(std::isfinite(stats.narrow_phase_ms));
        REQUIRE(stats.narrow_phase_ms >= 0);
        REQUIRE(std::isfinite(stats.solver_ms));
        REQUIRE(stats.solver_ms >= 0);
    }
}
TEST_CASE("Naive metrics distinguish candidates initial tests and correction retests") {
    World world;
    const auto a = world.create();
    const auto b = world.create();
    const auto missing = world.create();
    const auto fixed = world.create();
    for (const auto entity : {a, b, missing, fixed}) {
        world.set_collider(entity, SphereCollider(1));
    }
    world.set_transform(a);
    world.set_transform(b).position = {1.5F, 0, 0};
    world.set_transform(fixed).position = {20, 0, 0};
    world.set_rigid_body(a);
    // b and fixed have no rigid body, so they are static. missing has no transform.
    CollisionSystem collisions(BroadPhase::naive);
    CollisionStats stats;
    collisions.solve(world, &stats);
    REQUIRE(stats.candidate_pairs == 6);
    REQUIRE(stats.collision_checks == 2);
    REQUIRE(stats.contacts == 1);
    REQUIRE(stats.correction_checks == 4);
    REQUIRE(world.transform(a)->position.x < -0.49F);
}
