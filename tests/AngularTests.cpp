#include <coresim/physics/AngularDynamics.hpp>
#include <coresim/physics/PhysicsSystem.hpp>
#include <coresim/scene/World.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <stdexcept>
#include <tuple>
using namespace coresim;
TEST_CASE("Primitive inertia follows current mass and explicit collider dimensions") {
    RigidBody body(3);
    Collider shape = BoxCollider({1,2,3});
    auto t = inertia(body,&shape);
    REQUIRE(t.local == glm::vec3(13,10,5));
    REQUIRE(t.inverse_local.x == Catch::Approx(1.0F/13));
    body.set_mass(6);
    REQUIRE(inertia(body,&shape).local == glm::vec3(26,20,10));
    shape = SphereCollider(2);
    REQUIRE(inertia(body,&shape).local.x == Catch::Approx(9.6F));
    REQUIRE(inertia(body,nullptr).local == glm::vec3(6));
    body.set_mass(0);
    REQUIRE(inertia(body,&shape).inverse_local == glm::vec3(0));
    const auto q = glm::angleAxis(1.57079632679F,glm::vec3(0,0,1));
    const auto tensor = world_tensor({1,2,3},q);
    REQUIRE(tensor[0][0] == Catch::Approx(2).margin(1e-6));
    REQUIRE(tensor[1][1] == Catch::Approx(1).margin(1e-6));
    REQUIRE(tensor[2][2] == Catch::Approx(3).margin(1e-6));
}
TEST_CASE("Torque and a force at an offset advance angular motion and clear per tick") {
    World world; const auto e = world.create();
    world.set_transform(e); world.set_collider(e,BoxCollider(glm::vec3(1)));
    auto& body = world.set_rigid_body(e,RigidBody(3));
    body.add_force_at_offset({0,4,0},{1,0,0});
    REQUIRE(body.accumulated_torque() == glm::vec3(0,0,4));
    PhysicsSystem physics({0,0,0}); physics.step(world,0.1F);
    REQUIRE(body.angular_velocity.z == Catch::Approx(0.2F).margin(1e-6));
    REQUIRE(world.transform(e)->rotation.z == Catch::Approx(std::sin(0.01F)).margin(1e-6));
    REQUIRE(body.velocity.y == Catch::Approx(4.0F/3*0.1F));
    REQUIRE(body.accumulated_force() == glm::vec3(0));
    REQUIRE(body.accumulated_torque() == glm::vec3(0));
    physics.step(world,0.1F);
    REQUIRE(body.angular_velocity.z == Catch::Approx(0.2F).margin(1e-6));
    REQUIRE(glm::length(world.transform(e)->rotation) == Catch::Approx(1).margin(1e-6));
}
TEST_CASE("Asymmetric torque-free rotation preserves world momentum with bounded energy drift") {
    RigidBody body(2); body.angular_velocity = {1,2,3};
    Collider box = BoxCollider({1,2,3});
    auto q = glm::angleAxis(0.4F,glm::normalize(glm::vec3(1,2,3)));
    const auto t = inertia(body,&box);
    const auto momentum = world_tensor(t.local,q)*body.angular_velocity;
    const float energy = 0.5F*glm::dot(momentum,body.angular_velocity);
    for (int tick = 0; tick < 2400; ++tick) {
        const auto next = integrate_angular(body,&box,q,1.0F/120);
        q = next.rotation; body.angular_velocity = next.velocity;
        const auto current = world_tensor(t.local,q)*body.angular_velocity;
        REQUIRE(glm::length(current-momentum) < 0.015F);
        REQUIRE(std::abs(0.5F*glm::dot(current,body.angular_velocity)-energy) / energy < 0.01F);
        REQUIRE(glm::length(q) == Catch::Approx(1).margin(1e-6));
    }
}
TEST_CASE("Angular input validation preserves accumulated loads and body state") {
    RigidBody body; body.add_torque({1,2,3});
    const auto inf = std::numeric_limits<float>::infinity();
    REQUIRE_THROWS_AS(body.add_torque({inf,0,0}),std::invalid_argument);
    REQUIRE_THROWS_AS(body.add_force_at_offset({1,0,0},{0,inf,0}),std::invalid_argument);
    REQUIRE(body.accumulated_torque() == glm::vec3(1,2,3));
    REQUIRE(body.accumulated_force() == glm::vec3(0));
    REQUIRE_THROWS_AS(unit_rotation({0,0,0,0}),std::invalid_argument);
    REQUIRE_THROWS_AS(unit_rotation({inf,0,0,0}),std::invalid_argument);
    Collider tiny = SphereCollider(1e-30F);
    REQUIRE_THROWS_AS(inertia(body,&tiny),std::invalid_argument);
    World world; auto e = world.create(); world.set_transform(e); world.set_rigid_body(e).angular_velocity.x = inf;
    REQUIRE_THROWS_AS(PhysicsSystem{}.step(world,0.01F),std::runtime_error);
    REQUIRE(world.transform(e)->position == glm::vec3(0));
    REQUIRE(world.rigid_body(e)->velocity == glm::vec3(0));
}
TEST_CASE("Static and incomplete bodies clear torque without changing orientation") {
    World world; const auto e = world.create();
    const auto q = glm::angleAxis(0.4F,glm::vec3(0,0,1));
    world.set_transform(e).rotation = q;
    auto& b = world.set_rigid_body(e,RigidBody(0)); b.angular_velocity = {1,2,3}; b.add_torque({3,4,5});
    const auto incomplete = world.create(); world.set_rigid_body(incomplete).add_torque({3,4,5});
    PhysicsSystem{}.step(world,0.01F);
    REQUIRE(world.transform(e)->rotation == q);
    REQUIRE(world.rigid_body(e)->accumulated_torque() == glm::vec3(0));
    REQUIRE(world.rigid_body(incomplete)->accumulated_torque() == glm::vec3(0));
}

TEST_CASE("Oriented box contacts follow rotation and retain flat face patches") {
    const Collider box = BoxCollider({2,0.25F,0.25F});
    const auto q = glm::angleAxis(1.57079632679F,glm::vec3(0,0,1));
    REQUIRE_FALSE(detect_contact(box,{0,0,0},box,{0,3,0}));
    REQUIRE(detect_contact(box,{0,0,0},box,{0,3,0},q,q));
    REQUIRE_FALSE(detect_contact(box,{0,0,0},box,{3,0,0},q,q));
    const Collider cube = BoxCollider(glm::vec3(1));
    const auto patch = detect_manifold(cube,{0,0,0},cube,{2,0,0});
    REQUIRE(patch.count == 4);
    for (std::size_t i = 0; i < patch.count; ++i) {
        REQUIRE(patch.points[i].point.x == 1);
        REQUIRE(patch.points[i].normal == glm::vec3(1,0,0));
        REQUIRE(patch.points[i].penetration == 0);
    }
    const auto common = glm::angleAxis(0.6F,glm::normalize(glm::vec3(1,2,3)));
    const auto original = detect_contact(cube,{0,0,0},cube,{1.8F,0.1F,0.2F});
    const auto rotated = detect_contact(cube,{0,0,0},cube,common*glm::vec3(1.8F,0.1F,0.2F),common,common);
    REQUIRE(rotated);
    REQUIRE(rotated->penetration == Catch::Approx(original->penetration).margin(1e-5));
    REQUIRE(glm::length(rotated->normal-common*original->normal) < 1e-5F);
    const auto reversed = detect_contact(cube,common*glm::vec3(1.8F,0.1F,0.2F),cube,{0,0,0},common,common);
    REQUIRE(glm::length(rotated->normal+reversed->normal) < 1e-5F);
}
TEST_CASE("Corner impact matches angular effective mass and conserves elastic energy") {
    World world; const auto e = world.create();
    Transform t;
    t.rotation = glm::angleAxis(0.4F,glm::vec3(0,0,1))*glm::angleAxis(0.3F,glm::vec3(1,0,0));
    const Collider shape = BoxCollider({1,0.7F,0.5F}); world.set_collider(e,shape);
    const auto r = glm::mat3_cast(t.rotation); const auto h = std::get<BoxCollider>(shape).half_extents;
    t.position.y = glm::abs(r[0]).y*h.x+glm::abs(r[1]).y*h.y+glm::abs(r[2]).y*h.z-0.001F;
    world.set_transform(e,t);
    world.set_rigid_body(e,RigidBody(2,1,0)).velocity = {0,-4,0};
    const auto floor = world.create(); world.set_transform(floor); world.set_collider(floor,PlaneCollider{});
    world.set_rigid_body(floor,RigidBody(0,1));
    const auto patch = detect_manifold(shape,t.position,PlaneCollider{}, {},t.rotation);
    REQUIRE(patch.count == 1);
    const auto offset = patch.points[0].point-t.position;
    const auto crossed = glm::cross(offset,glm::vec3(0,1,0));
    const auto tensors = inertia(*world.rigid_body(e),&shape);
    const auto inv = world_tensor(tensors.inverse_local,t.rotation);
    const float impulse = 8/(0.5F+glm::dot(crossed,inv*crossed));
    CollisionSystem{}.solve(world);
    const auto& b = *world.rigid_body(e);
    REQUIRE(b.velocity.y == Catch::Approx(-4+0.5F*impulse).margin(1e-5));
    REQUIRE(glm::length(b.angular_velocity-inv*crossed*impulse) < 1e-5F);
    const float energy = glm::dot(b.velocity,b.velocity)+0.5F*glm::dot(b.angular_velocity,world_tensor(tensors.local,t.rotation)*b.angular_velocity);
    REQUIRE(energy == Catch::Approx(16).margin(1e-4));
}
TEST_CASE("Off-center box impacts conserve linear and angular momentum") {
    World world;
    for (int i = 0; i < 2; ++i) {
        const auto e = world.create(); world.set_transform(e).position = {static_cast<float>(i)*2,static_cast<float>(i)*0.7F,0};
        world.set_collider(e,BoxCollider{}); world.set_rigid_body(e,RigidBody(2,0)).velocity = {i == 0 ? 2.0F : -2.0F,0,0};
    }
    const auto totals = [&] {
        glm::vec3 linear(0),angular(0); float energy = 0;
        for (const auto& e : world.rigid_bodies()) {
            const auto& b = e.value; const auto& t = *world.transform(e.entity);
            const auto spin = world_tensor(inertia(b,world.collider(e.entity)).local,t.rotation)*b.angular_velocity;
            linear += b.mass()*b.velocity;
            angular += glm::cross(t.position,b.mass()*b.velocity)+spin;
            energy += 0.5F*b.mass()*glm::dot(b.velocity,b.velocity)+0.5F*glm::dot(spin,b.angular_velocity);
        }
        return std::tuple(linear,angular,energy);
    };
    const auto [p,l,energy] = totals();
    CollisionSystem{}.solve(world);
    const auto [after_p,after_l,after_energy] = totals();
    REQUIRE(glm::length(after_p-p) < 1e-5F);
    REQUIRE(glm::length(after_l-l) < 1e-4F);
    REQUIRE(after_energy <= energy+1e-4F);
    REQUIRE(std::abs(world.rigid_bodies()[0].value.angular_velocity.z) > 0.1F);
    REQUIRE(std::abs(world.rigid_bodies()[1].value.angular_velocity.z) > 0.1F);
}
TEST_CASE("Contact friction damps sliding and spin while zero friction preserves sliding") {
    for (float friction : {0.0F,0.5F}) {
        World world; const auto floor = world.create(); world.set_transform(floor);
        world.set_collider(floor,PlaneCollider{}); world.set_rigid_body(floor,RigidBody(0,0,friction));
        const auto box = world.create(); world.set_transform(box).position = {0,1,0};
        world.set_collider(box,BoxCollider{});
        auto& b = world.set_rigid_body(box,RigidBody(1,0,friction)); b.velocity = {2,0,0}; b.angular_velocity = {0,1,0};
        PhysicsSystem physics;
        for (int tick = 0; tick < 600; ++tick) { physics.step(world,1.0F/120); }
        if (friction == 0) { REQUIRE(b.velocity.x == Catch::Approx(2).margin(1e-5)); }
        else { REQUIRE(glm::length(b.velocity) < 0.02F); REQUIRE(glm::length(b.angular_velocity) < 0.02F); }
        REQUIRE(world.transform(box)->position.y > 0.99F);
    }
    RigidBody body;
    REQUIRE_THROWS_AS(body.set_friction(-0.1F),std::invalid_argument);
    REQUIRE_THROWS_AS(body.set_friction(std::numeric_limits<float>::quiet_NaN()),std::invalid_argument);
    REQUIRE(body.friction() == 0.5F);
}
TEST_CASE("Rotated plane contacts and skew rods use oriented geometry") {
    const Collider box = BoxCollider(glm::vec3(1)), plane = PlaneCollider{};
    const auto q = glm::angleAxis(1.57079632679F,glm::vec3(0,0,1));
    const auto contact = detect_contact(box,{-0.9F,0,0},plane,{},glm::quat(1,0,0,0),q);
    REQUIRE(contact);
    REQUIRE(contact->normal.x == Catch::Approx(1).margin(1e-5));
    const Collider rod = BoxCollider({2,0.1F,0.1F});
    const auto a = glm::angleAxis(0.4F,glm::vec3(0,1,0))*glm::angleAxis(0.5F,glm::vec3(0,0,1));
    const auto b = glm::angleAxis(0.6F,glm::vec3(1,0,0))*glm::angleAxis(-0.7F,glm::vec3(0,0,1));
    const auto direction = glm::normalize(glm::cross(a*glm::vec3(1,0,0),b*glm::vec3(1,0,0)));
    REQUIRE_FALSE(detect_contact(rod,{},rod,direction*0.4F,a,b));
    const auto touching = detect_manifold(rod,{},rod,direction*0.1F,a,b);
    REQUIRE(touching.count == 1); // Skew-edge contact, not a face patch.
    REQUIRE(glm::length(touching.points[0].normal) == Catch::Approx(1).margin(1e-5));
}
