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
