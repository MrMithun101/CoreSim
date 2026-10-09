#include <coresim/benchmark/LayoutBenchmark.hpp>
#include <coresim/physics/Integration.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <limits>
#include <stdexcept>
using namespace coresim;
TEST_CASE("Packed and dense integration preserve the lookup path across ticks and storage churn") {
    for (auto layout : {IntegrationLayout::dense, IntegrationLayout::aos, IntegrationLayout::soa}) {
        World reference, alternative;
        make_layout_world(reference, 64, true); make_layout_world(alternative, 64, true);
        LayoutExperiment a,b;
        for (int tick = 0; tick < 30; ++tick) {
            if (tick == 10) {
                for (World* world : {&reference, &alternative}) {
                    const auto entity = world->rigid_bodies()[7].entity;
                    REQUIRE(world->destroy(entity));
                    const auto created = world->create();
                    world->set_rigid_body(created, RigidBody(2));
                    world->set_transform(created).position = {10,20,30};
                    world->set_collider(created, SphereCollider(1));
                    const auto incomplete = world->rigid_bodies()[8].entity;
                    world->remove_transform(incomplete);
                }
            }
            a.step(reference, IntegrationLayout::lookup, 1.0F/120);
            const auto timing = b.step(alternative, layout, 1.0F/120);
            REQUIRE(timing.total_ms >= 0);
            const auto ra = reference.rigid_bodies(), rb = alternative.rigid_bodies();
            REQUIRE(ra.size() == rb.size());
            for (std::size_t i = 0; i < ra.size(); ++i) {
                REQUIRE(ra[i].entity == rb[i].entity);
                REQUIRE(rb[i].value.accumulated_force() == glm::vec3(0));
                const auto* pa = reference.transform(ra[i].entity);
                const auto* pb = alternative.transform(rb[i].entity);
                REQUIRE((pa != nullptr) == (pb != nullptr));
                for (int axis = 0; axis < 3; ++axis) {
                    REQUIRE(rb[i].value.velocity[axis] == Catch::Approx(ra[i].value.velocity[axis]).margin(1e-6));
                    if (pa && pb) { REQUIRE(pb->position[axis] == Catch::Approx(pa->position[axis]).margin(1e-5)); }
                }
            }
        }
    }
}
TEST_CASE("Integration variants reject invalid inputs and dynamic planes") {
    for (auto layout : {IntegrationLayout::lookup, IntegrationLayout::dense, IntegrationLayout::aos, IntegrationLayout::soa}) {
        World world; LayoutExperiment experiment;
        REQUIRE_THROWS_AS(experiment.step(world, layout, 0), std::invalid_argument);
        REQUIRE_THROWS_AS(experiment.step(world, layout, std::numeric_limits<float>::infinity()), std::invalid_argument);
        const auto e = world.create(); world.set_transform(e); world.set_rigid_body(e); world.set_collider(e, PlaneCollider{});
        REQUIRE_THROWS_AS(experiment.step(world, layout, 1.0F/120), std::invalid_argument);
        world.remove_collider(e);
        world.rigid_body(e)->velocity.x = std::numeric_limits<float>::infinity();
        REQUIRE_THROWS_AS(experiment.step(world, layout, 1.0F/120), std::runtime_error);
    }
}
