#include <coresim/scene/DemoScene.hpp>
#include <coresim/core/FixedStepper.hpp>
#include <coresim/physics/PhysicsSystem.hpp>
#include <coresim/scene/World.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace coresim;
namespace {
Entity dynamic_body(World& world, float mass = 1.0F) {
    const auto entity = world.create();
    world.set_transform(entity);
    world.set_rigid_body(entity, RigidBody(mass));
    return entity;
}
}

TEST_CASE("Semi-implicit integration combines gravity acceleration and accumulated force") {
    World world;
    const auto entity = dynamic_body(world, 2.0F);
    auto& body = *world.rigid_body(entity);
    body.acceleration = {1, 0, 0};
    body.add_force({2, 0, 0});
    body.add_force({4, 0, 0});
    PhysicsSystem physics;
    physics.step(world, 0.1F);
    REQUIRE(body.velocity.x == Catch::Approx(0.4F));
    REQUIRE(body.velocity.y == Catch::Approx(-0.981F));
    REQUIRE(world.transform(entity)->position.x == Catch::Approx(0.04F));
    REQUIRE(world.transform(entity)->position.y == Catch::Approx(-0.0981F));
    REQUIRE(body.accumulated_force() == glm::vec3(0));
    physics.step(world, 0.1F);
    REQUIRE(body.velocity.x == Catch::Approx(0.5F));
    REQUIRE(body.acceleration.x == 1.0F);
}

TEST_CASE("Gravity is mass independent while force response uses inverse mass") {
    World world;
    const auto light = dynamic_body(world, 1);
    const auto heavy = dynamic_body(world, 4);
    world.rigid_body(light)->add_force({8, 0, 0});
    world.rigid_body(heavy)->add_force({8, 0, 0});
    PhysicsSystem{}.step(world, 0.125F);
    REQUIRE(world.rigid_body(light)->velocity.y == world.rigid_body(heavy)->velocity.y);
    REQUIRE(world.rigid_body(light)->velocity.x == Catch::Approx(1));
    REQUIRE(world.rigid_body(heavy)->velocity.x == Catch::Approx(0.25F));
}

TEST_CASE("Static and incomplete entities do not integrate and do not retain old forces") {
    World world;
    const auto stationary = dynamic_body(world, 0);
    world.rigid_body(stationary)->velocity = {10, 10, 10};
    world.rigid_body(stationary)->add_force({1, 2, 3});
    const auto incomplete = world.create();
    world.set_rigid_body(incomplete).add_force({3, 4, 5});
    PhysicsSystem{}.step(world, 0.01F);
    REQUIRE(world.transform(stationary)->position == glm::vec3(0));
    REQUIRE(world.rigid_body(stationary)->accumulated_force() == glm::vec3(0));
    REQUIRE(world.rigid_body(incomplete)->accumulated_force() == glm::vec3(0));
    REQUIRE(world.destroy(stationary));
    REQUIRE(world.rigid_body(stationary) == nullptr);
    const auto replacement = world.create();
    REQUIRE(replacement.index == stationary.index);
    REQUIRE(world.rigid_body(replacement) == nullptr);
    REQUIRE_THROWS_AS(world.set_rigid_body(stationary), std::invalid_argument);
    REQUIRE(world.remove_rigid_body(incomplete));
    REQUIRE_FALSE(world.remove_rigid_body(incomplete));
    REQUIRE(world.rigid_bodies().empty());
}

TEST_CASE("Fixed stepping produces the same simulation at different render rates") {
    glm::vec3 reference_position{};
    glm::vec3 reference_velocity{};
    for (const int rate : {30, 60, 144}) {
        World world;
        const auto entity = dynamic_body(world, 2);
        FixedStepper clock;
        PhysicsSystem physics;
        unsigned int ticks = 0;
        for (int frame = 0; frame < rate * 2; ++frame) {
            const auto result = clock.advance(1.0 / rate, [&](float dt) {
                world.rigid_body(entity)->add_force({2, 0, 0});
                physics.step(world, dt);
            });
            ticks += result.steps;
            REQUIRE(result.dropped_seconds == 0);
        }
        REQUIRE(ticks == 240);
        REQUIRE(clock.remainder_seconds() < FixedStepper::step_seconds);
        const auto position = world.transform(entity)->position;
        const auto velocity = world.rigid_body(entity)->velocity;
        const double h = FixedStepper::step_seconds;
        REQUIRE(position.y == Catch::Approx(-9.81 * h * h * 240 * 241 / 2).epsilon(0.0001));
        REQUIRE(velocity.y == Catch::Approx(-19.62).epsilon(0.0001));
        if (rate == 30) {
            reference_position = position;
            reference_velocity = velocity;
        } else {
            REQUIRE(position == reference_position);
            REQUIRE(velocity == reference_velocity);
        }
    }
}

TEST_CASE("Fixed-step remainder retains pending forces and stalls drop bounded time") {
    World world;
    const auto entity = dynamic_body(world);
    world.rigid_body(entity)->add_force({12, 0, 0});
    FixedStepper clock;
    PhysicsSystem physics({0, 0, 0});
    auto tick = [&](float dt) { physics.step(world, dt); };
    REQUIRE(clock.advance(FixedStepper::step_seconds * 0.5, tick).steps == 0);
    REQUIRE(world.rigid_body(entity)->accumulated_force().x == 12);
    REQUIRE(clock.advance(FixedStepper::step_seconds * 0.5, tick).steps == 1);
    REQUIRE(world.rigid_body(entity)->velocity.x == Catch::Approx(0.1F));
    REQUIRE(world.rigid_body(entity)->accumulated_force() == glm::vec3(0));
    REQUIRE(clock.advance(FixedStepper::step_seconds, tick).steps == 1);
    REQUIRE(world.rigid_body(entity)->velocity.x == Catch::Approx(0.1F));
    const auto stalled = clock.advance(1.0, tick);
    REQUIRE(stalled.steps == FixedStepper::max_steps);
    REQUIRE(stalled.dropped_seconds == Catch::Approx(1.0 - 16.0 / 120.0));
    REQUIRE(clock.remainder_seconds() < FixedStepper::step_seconds);
    clock.reset();
    REQUIRE(clock.remainder_seconds() == 0);
}

TEST_CASE("Invalid physics inputs fail explicitly without corrupting validated properties") {
    RigidBody body(2, 0.6F);
    REQUIRE(body.mass() == 2);
    REQUIRE(body.inverse_mass() == 0.5F);
    REQUIRE(body.restitution() == 0.6F);
    REQUIRE_THROWS_AS(body.set_mass(-1), std::invalid_argument);
    REQUIRE(body.mass() == 2);
    REQUIRE_THROWS_AS(body.set_mass(std::numeric_limits<float>::infinity()), std::invalid_argument);
    REQUIRE_THROWS_AS(body.set_restitution(1.1F), std::invalid_argument);
    REQUIRE_THROWS_AS(body.add_force({0, std::numeric_limits<float>::quiet_NaN(), 0}), std::invalid_argument);
    REQUIRE(body.accumulated_force() == glm::vec3(0));
    World world;
    const auto entity = dynamic_body(world);
    REQUIRE_THROWS_AS(PhysicsSystem{}.step(world, 0), std::invalid_argument);
    REQUIRE_THROWS_AS(PhysicsSystem{}.step(world, -1), std::invalid_argument);
    FixedStepper clock;
    const auto no_op = [](float) {};
    REQUIRE_THROWS_AS(clock.advance(-1, no_op), std::invalid_argument);
    REQUIRE_THROWS_AS(clock.advance(std::numeric_limits<double>::quiet_NaN(), no_op), std::invalid_argument);
    REQUIRE(clock.remainder_seconds() == 0);
    world.rigid_body(entity)->velocity.x = std::numeric_limits<float>::infinity();
    REQUIRE_THROWS_AS(PhysicsSystem{}.step(world, 0.01F), std::runtime_error);
    REQUIRE(world.transform(entity)->position == glm::vec3(0));
}


TEST_CASE("Falling-body demo uses static references and resets motion without reviving stale IDs") {
    DemoScene scene;
    auto& world = scene.world();
    unsigned int dynamic = 0, stationary = 0;
    for (const auto& entry : world.rigid_bodies()) {
        if (entry.value.inverse_mass() > 0) { ++dynamic; } else { ++stationary; }
    }
    REQUIRE(dynamic == 100);
    REQUIRE(stationary == 2);
    const auto moving = world.rigid_bodies()[0].entity;
    const auto fixed = world.rigid_bodies()[100].entity;
    const auto initial_position = world.transform(moving)->position;
    const auto initial_velocity = world.rigid_body(moving)->velocity;
    const auto fixed_position = world.transform(fixed)->position;
    PhysicsSystem physics;
    for (int tick = 0; tick < 240; ++tick) {
        physics.step(world, static_cast<float>(FixedStepper::step_seconds));
    }
    REQUIRE(world.transform(moving)->position.y < initial_position.y);
    REQUIRE(world.transform(fixed)->position == fixed_position);
    world.rigid_body(moving)->add_force({1, 2, 3});
    scene.reset_physics();
    REQUIRE(world.transform(moving)->position == initial_position);
    REQUIRE(world.rigid_body(moving)->velocity == initial_velocity);
    REQUIRE(world.rigid_body(moving)->accumulated_force() == glm::vec3(0));
    REQUIRE(world.destroy(moving));
    const auto replacement = dynamic_body(world);
    world.transform(replacement)->position = {42, 42, 42};
    scene.reset_physics();
    REQUIRE(world.transform(replacement)->position == glm::vec3(42));
}

TEST_CASE("Irregular render durations retain fixed-tick integration") {
    World world;
    const auto entity = dynamic_body(world);
    FixedStepper clock;
    PhysicsSystem physics;
    constexpr double frames[]{0.001, 0.02, 0.017, 0.009, 0.041};
    double elapsed = 0;
    unsigned int count = 0, ticks = 0;
    while (elapsed < 2.0) {
        const double dt = std::min(frames[count++ % 5], 2.0 - elapsed);
        elapsed += dt;
        const auto result = clock.advance(dt, [&](float step) { physics.step(world, step); });
        ticks += result.steps;
        REQUIRE(result.dropped_seconds == 0);
    }
    REQUIRE(ticks == 240);
    REQUIRE(world.rigid_body(entity)->velocity.y == Catch::Approx(-19.62).epsilon(0.0001));
}
