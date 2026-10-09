#include <coresim/scene/Camera.hpp>
#include <coresim/scene/DemoScene.hpp>
#include <coresim/scene/Transform.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace coresim;
TEST_CASE("Camera movement uses local axes without diagonal speed gain") {
    Camera forward;
    forward.move({0, 0, 1}, 1.0F);
    REQUIRE(forward.position().z == Catch::Approx(-3.0F));
    Camera diagonal;
    diagonal.move({1, 0, 1}, 1.0F);
    REQUIRE(glm::length(diagonal.position() - glm::vec3(0, 0, 5)) == Catch::Approx(8.0F));
    Camera split;
    for (int i = 0; i < 100; ++i) {
        split.move({0, 0, 1}, 0.01F);
    }
    REQUIRE(glm::length(split.position() - forward.position()) < 0.0001F);
    Camera turned;
    turned.look(glm::half_pi<float>(), 0);
    turned.move({0, 0, 1}, 1.0F);
    REQUIRE(turned.position().x == Catch::Approx(8.0F));
}
TEST_CASE("Camera view maps its position to the origin and look stays finite at poles") {
    Camera camera({3, 4, 5});
    const auto eye = camera.view() * glm::vec4(camera.position(), 1);
    REQUIRE(glm::length(glm::vec3(eye)) < 0.00001F);
    camera.look(10000, 10000);
    REQUIRE(glm::length(camera.direction()) == Catch::Approx(1.0F));
    REQUIRE(camera.direction().y < 1.0F);
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            REQUIRE(std::isfinite(camera.view()[column][row]));
        }
    }
    const auto wide = camera.projection(2);
    const auto square = camera.projection(1);
    REQUIRE(wide[0][0] == Catch::Approx(square[0][0] / 2));
    REQUIRE(wide[1][1] == Catch::Approx(square[1][1]));
    REQUIRE_THROWS_AS(camera.projection(0), std::invalid_argument);
    REQUIRE_THROWS_AS(camera.move({0, 0, 1}, -1), std::invalid_argument);
    REQUIRE_THROWS_AS(camera.look(std::numeric_limits<float>::infinity(), 0), std::invalid_argument);
}
TEST_CASE("Transform scales then rotates then translates independently") {
    Transform first;
    first.position = {10, 0, 0};
    first.scale = {2, 3, 4};
    first.rotation = glm::angleAxis(glm::half_pi<float>(), glm::vec3(0, 0, 1));
    const auto point = first.matrix() * glm::vec4(1, 0, 0, 1);
    REQUIRE(point.x == Catch::Approx(10));
    REQUIRE(point.y == Catch::Approx(2));
    REQUIRE(point.z == Catch::Approx(0).margin(0.00001));
    const Transform second;
    REQUIRE(second.matrix() * glm::vec4(1, 2, 3, 1) == glm::vec4(1, 2, 3, 1));
}


TEST_CASE("Collision demo has deterministic transforms and stable orientations") {
    DemoScene scene;
    const DemoScene copy;
    REQUIRE(scene.world().transforms().size() == 102);
    for (std::size_t i = 0; i < scene.world().transforms().size(); ++i) {
        REQUIRE(scene.world().transforms()[i].value.position == copy.world().transforms()[i].value.position);
        if (i > 0) {
            REQUIRE(scene.world().transforms()[i].value.position != scene.world().transforms()[i - 1].value.position);
        }
    }
    for (int frame = 0; frame < 1000; ++frame) {
        scene.update(0.016F);
    }
    for (std::size_t i = 0; i < scene.world().transforms().size(); ++i) {
        REQUIRE(scene.world().transforms()[i].value.position == copy.world().transforms()[i].value.position);
        REQUIRE(glm::length(scene.world().transforms()[i].value.rotation) == Catch::Approx(1.0F));
    }
    REQUIRE(scene.world().transforms()[0].value.position != scene.world().transforms()[1].value.position);
    REQUIRE_THROWS_AS(scene.update(-1), std::invalid_argument);
}

TEST_CASE("Demo updates survive entity deletion and incomplete component combinations") {
    DemoScene scene;
    auto& world = scene.world();
    const auto removed = world.transforms()[0].entity;
    const auto missing_transform = world.transforms()[1].entity;
    world.set_spin(removed, {{0, 1, 0}, 1});
    world.set_spin(missing_transform, {{0, 1, 0}, 1});
    REQUIRE(world.destroy(removed));
    REQUIRE(world.remove_transform(missing_transform));
    const auto static_entity = world.create();
    const auto initial = world.set_transform(static_entity).rotation;
    world.set_mesh(static_entity);
    REQUIRE_NOTHROW(scene.update(0.1F));
    REQUIRE(world.transform(static_entity)->rotation == initial);
    REQUIRE_FALSE(world.alive(removed));
    REQUIRE(world.transform(missing_transform) == nullptr);
    REQUIRE(world.size() == 102);
}
