#include <coresim/benchmark/LayoutBenchmark.hpp>
#include <coresim/jobs/JobSystem.hpp>
#include <catch2/catch_test_macros.hpp>
#include <future>
#include <limits>
#include <stdexcept>
#include <type_traits>

using namespace coresim;
namespace {
void compare(const World& a, const World& b) {
    const auto aa = a.rigid_bodies(), bb = b.rigid_bodies();
    REQUIRE(aa.size() == bb.size());
    for (std::size_t i = 0; i < aa.size(); ++i) {
        REQUIRE(aa[i].entity == bb[i].entity);
        REQUIRE(aa[i].value.velocity == bb[i].value.velocity);
        REQUIRE(aa[i].value.angular_velocity == bb[i].value.angular_velocity);
        REQUIRE(aa[i].value.accumulated_torque() == bb[i].value.accumulated_torque());
        REQUIRE(aa[i].value.accumulated_force() == bb[i].value.accumulated_force());
        const auto* ta = a.transform(aa[i].entity);
        const auto* tb = b.transform(bb[i].entity);
        REQUIRE(bool(ta) == bool(tb));
        if (ta) { REQUIRE(ta->position == tb->position); REQUIRE(ta->rotation == tb->rotation); }
    }
}
}
TEST_CASE("Parallel ranges preserve exact integration through joins and structural churn") {
    for (auto workers : {1U, 2U, 4U, 8U}) {
        JobSystem jobs(workers);
        for (const auto iteration : {BodyIteration::lookup, BodyIteration::dense}) {
            World reference, parallel;
            make_layout_world(reference, 2053, true); make_layout_world(parallel, 2053, true);
            for (auto* world : {&reference,&parallel}) {
                world->for_each_rigid_body([](Entity, RigidBody& body) {
                    body.angular_velocity = {0.3F,0.5F,-0.2F}; body.add_torque({0.1F,-0.3F,0.2F});
                });
            }
            for (int tick = 0; tick < 8; ++tick) {
                if (tick == 3) {
                    for (auto* world : {&reference, &parallel}) {
                        world->destroy(world->rigid_bodies()[9].entity);
                        const auto e = world->create();
                        world->set_rigid_body(e, RigidBody(2)); world->set_transform(e);
                        world->remove_transform(world->rigid_bodies()[11].entity);
                    }
                }
                integrate_bodies(reference, {0,-9.81F,0}, 1.0F/120);
                integrate_bodies(parallel, {0,-9.81F,0}, 1.0F/120, iteration, &jobs, 64);
                compare(reference, parallel);
            }
        }
    }
}
TEST_CASE("Parallel integration joins before deterministic contact solving") {
    for (auto mode : {BroadPhase::naive, BroadPhase::spatial_hash}) {
        for (auto workers : {1U, 2U, 4U, 8U}) {
            JobSystem jobs(workers);
            World a,b;
            for (auto* world : {&a,&b}) {
                for (int i = 0; i < 40; ++i) {
                    const auto e = world->create();
                    world->set_transform(e).position = {static_cast<float>(i%5)*1.5F, static_cast<float>(i/5)*1.5F,0};
                    world->set_rigid_body(e);
                    world->set_collider(e, i%2 ? Collider(SphereCollider(1)) : Collider(BoxCollider(glm::vec3(1))));
                }
                const auto e = world->create(); world->set_transform(e).position = {0,-2,0};
                world->set_collider(e, PlaneCollider{}); world->set_rigid_body(e, RigidBody(0));
            }
            PhysicsSystem serial({0,-9.81F,0},mode), parallel({0,-9.81F,0},mode);
            for (int tick = 0; tick < 60; ++tick) {
                CollisionStats sa,sb;
                serial.step(a,1.0F/120,&sa); parallel.step(b,1.0F/120,&sb,nullptr,&jobs,4);
                REQUIRE(sa.contacts == sb.contacts);
                REQUIRE(sa.candidate_pairs == sb.candidate_pairs);
                REQUIRE(sa.collision_checks == sb.collision_checks);
                compare(a,b);
            }
        }
    }
}
TEST_CASE("Parallel integration validates before submission and drains failures") {
    World world; make_layout_world(world, 128, false);
    JobSystem jobs(4);
    REQUIRE_THROWS_AS(integrate_bodies(world, {}, 0, BodyIteration::lookup, &jobs, 1), std::invalid_argument);
    REQUIRE_THROWS_AS(integrate_bodies(world, {}, 1, BodyIteration::lookup, &jobs, 0), std::invalid_argument);
    REQUIRE_THROWS_AS(integrate_bodies(world, {std::numeric_limits<float>::infinity(),0,0}, 1,
                                     BodyIteration::lookup, &jobs, 1), std::invalid_argument);
    const auto bad = world.rigid_bodies()[1].entity;
    world.set_collider(bad, PlaneCollider{});
    REQUIRE_THROWS_AS(integrate_bodies(world, {}, 1.0F/120, BodyIteration::lookup, &jobs, 1), std::invalid_argument);
    // The failing range may stop early; every other range must finish before the throw.
    for (std::size_t i = 32; i < 128; ++i) { REQUIRE(world.rigid_bodies()[i].value.accumulated_force() == glm::vec3(0)); }
    world.remove_collider(bad);
    world.rigid_body(bad)->velocity.x = std::numeric_limits<float>::infinity();
    REQUIRE_THROWS_AS(integrate_bodies(world, {}, 1, BodyIteration::lookup, &jobs, 1), std::runtime_error);
    jobs.shutdown();
    REQUIRE_THROWS_AS(integrate_bodies(world, {}, 1, BodyIteration::lookup, &jobs, 1), std::runtime_error);
}
TEST_CASE("Small and empty worlds use inline integration without pool admission") {
    JobSystem jobs(2); jobs.shutdown();
    World a,b;
    integrate_bodies(a, {}, 1, BodyIteration::lookup, &jobs);
    make_layout_world(a, 17, false); make_layout_world(b, 17, false);
    integrate_bodies(a, {}, 1, BodyIteration::lookup, &jobs, 18);
    integrate_bodies(b, {}, 1);
    compare(a,b);
}
TEST_CASE("Nested synchronous integration is rejected before any world writes") {
    JobSystem jobs(2);
    World a,b; make_layout_world(a,128,false); make_layout_world(b,128,false);
    auto group = jobs.submit([&] { integrate_bodies(a, {}, 1, BodyIteration::lookup, &jobs,1); });
    REQUIRE_THROWS_AS(group.wait(), std::logic_error);
    compare(a,b);
    REQUIRE_FALSE(JobSystem::is_worker_thread());
}
TEST_CASE("Concurrent pool shutdown leaves no outstanding physics borrows") {
    for (int repeat = 0; repeat < 30; ++repeat) {
        JobSystem jobs(8);
        auto world = std::make_unique<World>(); make_layout_world(*world,2048,true);
        auto closer = std::async(std::launch::async, [&] { jobs.shutdown(); });
        try { integrate_bodies(*world, {}, 1, BodyIteration::lookup, &jobs, 1); }
        catch (const std::runtime_error&) {} // Admission can race with closure.
        world.reset(); // ASan/TSan must find no queued or executing job borrowing this world.
        closer.get();
    }
}
TEST_CASE("Bounded dense visitors protect IDs and reject invalid ranges") {
    World world; make_layout_world(world, 5, false);
    unsigned count = 0;
    world.for_each_rigid_body_range(1,4,[&](auto&& id, RigidBody& body) {
        static_assert(std::is_const_v<std::remove_reference_t<decltype(id)>>);
        body.velocity = {1,2,3}; ++count;
    });
    REQUIRE(count == 3);
    REQUIRE_THROWS_AS(world.for_each_rigid_body_range(4,3,[](Entity, RigidBody&) {}), std::out_of_range);
    REQUIRE_THROWS_AS(world.for_each_rigid_body_range(0,6,[](Entity, RigidBody&) {}), std::out_of_range);
}
