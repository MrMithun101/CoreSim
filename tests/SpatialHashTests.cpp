#include <coresim/physics/SpatialHash.hpp>
#include <coresim/physics/PhysicsSystem.hpp>
#include <coresim/scene/World.hpp>
#include <coresim/scene/DemoScene.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <limits>
#include <random>
#include <set>
#include <stdexcept>

using namespace coresim;
namespace {
Entity add(World& world, glm::vec3 position, const Collider& shape, float mass = 1) {
    const auto entity = world.create();
    world.set_transform(entity).position = position;
    world.set_collider(entity, shape);
    world.set_rigid_body(entity, RigidBody(mass));
    return entity;
}
void check_candidates(World& world, float cell_size) {
    SpatialHash grid(cell_size);
    grid.build(world);
    const auto colliders = world.colliders();
    for (std::size_t i = 0; i < colliders.size(); ++i) {
        const auto candidates = grid.query(i);
        REQUIRE(std::is_sorted(candidates.begin(), candidates.end()));
        REQUIRE(std::adjacent_find(candidates.begin(), candidates.end()) == candidates.end());
        for (const auto j : candidates) { REQUIRE(j > i); REQUIRE(j < colliders.size()); }
        const std::vector<std::size_t> saved(candidates.begin(), candidates.end());
        const auto repeated = grid.query(i);
        REQUIRE(std::equal(saved.begin(), saved.end(), repeated.begin(), repeated.end()));
        for (std::size_t j = i + 1; j < colliders.size(); ++j) {
            const auto* a = world.transform(colliders[i].entity);
            const auto* b = world.transform(colliders[j].entity);
            if (a && b && detect_contact(colliders[i].value, a->position, colliders[j].value, b->position)) {
                REQUIRE(std::binary_search(repeated.begin(), repeated.end(), j));
            }
        }
    }
}
}
TEST_CASE("Grid candidates conservatively contain every supported primitive contact") {
    World world;
    std::mt19937 random(1843);
    std::uniform_real_distribution<float> position(-12, 12), extent(0.1F, 4);
    for (int i = 0; i < 120; ++i) {
        const glm::vec3 p(position(random), position(random), position(random));
        if (i % 2 == 0) { add(world, p, SphereCollider(extent(random))); }
        else { add(world, p, BoxCollider({extent(random), extent(random), extent(random)})); }
    }
    add(world, {0, 0, 0}, PlaneCollider{}, 0);
    add(world, {-3, 2, 1}, PlaneCollider({1,2,3}, 2), 0);
    // Exact touching and negative coordinates/cell boundaries.
    for (float x : {-6.0F, -3.0F, 0.0F, 3.0F}) { add(world, {x,0,0}, SphereCollider(1.5F)); }
    for (float size : {0.5F, 3.0F, 11.0F}) { check_candidates(world, size); }
}
TEST_CASE("Grid handles global fallbacks rebuilds missing transforms and sparse pruning") {
    World world;
    add(world, {0,0,0}, BoxCollider(glm::vec3(100000)), 0);
    add(world, {0,0,0}, SphereCollider(1));
    add(world, {1.0e20F,0,0}, SphereCollider(1));
    add(world, {1.0e20F,0,0}, SphereCollider(1));
    const auto missing = world.create();
    world.set_collider(missing, SphereCollider(1));
    check_candidates(world, 3);
    SpatialHash grid;
    grid.build(world);
    REQUIRE(grid.query(4).empty());
    REQUIRE_THROWS_AS(grid.query(5), std::out_of_range);
    REQUIRE(world.destroy(missing));
    std::vector<Entity> entities;
    for (const auto& e : world.colliders()) { entities.push_back(e.entity); }
    for (const auto e : entities) { REQUIRE(world.destroy(e)); }
    const auto a = add(world, {-100,0,0}, SphereCollider(1));
    const auto b = add(world, {100,0,0}, SphereCollider(1));
    grid.build(world);
    REQUIRE(grid.query(0).empty());
    world.transform(b)->position = world.transform(a)->position;
    grid.build(world);
    REQUIRE(grid.query(0).size() == 1);
    REQUIRE(grid.query(0)[0] == 1);
    REQUIRE(world.destroy(a));
    grid.build(world);
    REQUIRE(grid.query(0).empty());
    REQUIRE_THROWS_AS(SpatialHash(0), std::invalid_argument);
    REQUIRE_THROWS_AS(SpatialHash(-1), std::invalid_argument);
    REQUIRE_THROWS_AS(SpatialHash(std::numeric_limits<float>::infinity()), std::invalid_argument);
}
TEST_CASE("Spatial and naive solvers preserve identical mixed scene trajectories") {
    DemoScene naive_scene, hash_scene;
    PhysicsSystem naive({0,-9.81F,0}, BroadPhase::naive);
    PhysicsSystem hashed({0,-9.81F,0}, BroadPhase::spatial_hash);
    CollisionStats ns, hs;
    for (int tick = 0; tick < 600; ++tick) {
        naive.step(naive_scene.world(), 1.0F / 120, &ns);
        hashed.step(hash_scene.world(), 1.0F / 120, &hs);
        REQUIRE(hs.contacts == ns.contacts);
        REQUIRE(hs.correction_checks == ns.correction_checks);
        REQUIRE(hs.candidate_pairs <= ns.candidate_pairs);
        const auto a = naive_scene.world().transforms();
        const auto b = hash_scene.world().transforms();
        REQUIRE(a.size() == b.size());
        for (std::size_t i = 0; i < a.size(); ++i) {
            REQUIRE(a[i].value.position == b[i].value.position);
            const auto* ab = naive_scene.world().rigid_body(a[i].entity);
            const auto* bb = hash_scene.world().rigid_body(b[i].entity);
            if (ab && bb) { REQUIRE(ab->velocity == bb->velocity); }
        }
    }
}
TEST_CASE("Hash metrics report reduction and reset without losing real contacts") {
    World world;
    add(world, {0,0,0}, SphereCollider(1));
    add(world, {1,0,0}, SphereCollider(1));
    add(world, {100,0,0}, SphereCollider(1));
    CollisionSystem system;
    CollisionStats stats;
    system.solve(world, &stats);
    REQUIRE(stats.candidate_pairs == 1);
    REQUIRE(stats.contacts == 1);
    REQUIRE(stats.pair_reduction_percent > 66);
    REQUIRE(stats.pair_reduction_percent < 67);
    REQUIRE(stats.hash_build_ms >= 0);
    REQUIRE(stats.query_ms >= 0);
    REQUIRE(stats.broad_phase_ms == stats.hash_build_ms + stats.query_ms);
    REQUIRE(stats.total_collision_ms >= stats.broad_phase_ms);
    World empty;
    system.solve(empty, &stats);
    REQUIRE(stats.candidate_pairs == 0);
    REQUIRE(stats.pair_reduction_percent == 0);
}
