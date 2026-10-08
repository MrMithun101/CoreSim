#include <coresim/scene/World.hpp>

#include <catch2/catch_test_macros.hpp>
#include <map>
#include <random>
#include <stdexcept>
#include <vector>

using namespace coresim;

TEST_CASE("Destroyed IDs remain invalid after slot reuse") {
    World world;
    REQUIRE_FALSE(world.alive({}));
    REQUIRE_FALSE(world.destroy({}));
    const auto first = world.create();
    world.set_transform(first).position.x = 42;
    world.set_mesh(first);
    world.set_spin(first);
    REQUIRE(world.destroy(first));
    REQUIRE_FALSE(world.destroy(first));
    REQUIRE(world.transform(first) == nullptr);
    REQUIRE(world.mesh(first) == nullptr);
    REQUIRE(world.spin(first) == nullptr);
    const auto replacement = world.create();
    REQUIRE(replacement.index == first.index);
    REQUIRE(replacement.generation != first.generation);
    REQUIRE(world.alive(replacement));
    REQUIRE_FALSE(world.alive(first));
    REQUIRE(world.transform(replacement) == nullptr);
    REQUIRE(world.mesh(replacement) == nullptr);
    REQUIRE(world.spin(replacement) == nullptr);
    REQUIRE_THROWS_AS(world.set_transform(first), std::invalid_argument);
    REQUIRE_THROWS_AS(world.set_mesh(first), std::invalid_argument);
    REQUIRE_THROWS_AS(world.set_spin(first), std::invalid_argument);
    REQUIRE_FALSE(world.remove_transform(first));
    REQUIRE_FALSE(world.remove_mesh(first));
    REQUIRE_FALSE(world.remove_spin(first));
    REQUIRE(world.size() == 1);
}

TEST_CASE("Components attach independently and dense compaction preserves surviving IDs") {
    World world;
    const auto a = world.create();
    const auto b = world.create();
    const auto c = world.create();
    world.set_transform(a).position.x = 1;
    world.set_transform(b).position.x = 2;
    world.set_transform(c).position.x = 3;
    world.set_mesh(b);
    world.set_spin(c).radians_per_second = 4;
    REQUIRE(world.remove_transform(b));
    REQUIRE_FALSE(world.remove_transform(b));
    REQUIRE(world.alive(b));
    REQUIRE(world.mesh(b) != nullptr);
    REQUIRE(world.transforms().size() == 2);
    REQUIRE(world.transform(c)->position.x == 3);
    REQUIRE(world.transform(a)->position.x == 1);
    // Replacing an existing component cannot add a duplicate dense entry.
    Transform replacement;
    replacement.position.x = 9;
    world.set_transform(c, replacement);
    REQUIRE(world.transforms().size() == 2);
    REQUIRE(world.transform(c)->position.x == 9);
    // Attaching from another component remains valid even if the pool reallocates.
    world.set_transform(b, *world.transform(c));
    REQUIRE(world.transform(b)->position.x == 9);
    REQUIRE(world.destroy(a));
    REQUIRE(world.transform(b)->position.x == 9);
    REQUIRE(world.transform(c)->position.x == 9);
    REQUIRE(world.spin(c)->radians_per_second == 4);
    REQUIRE(world.remove_spin(c));
    REQUIRE(world.remove_mesh(b));
    const World& read_only = world;
    REQUIRE(read_only.transform(c) != nullptr);
    REQUIRE(read_only.mesh(b) == nullptr);
    REQUIRE(read_only.spin(c) == nullptr);
}

TEST_CASE("Random entity and component churn agrees with a simple reference model") {
    struct Expected {
        Entity id;
        bool transform{};
        bool mesh{};
        bool spin{};
        float value{};
    };
    World world;
    std::vector<Expected> live;
    std::vector<Entity> dead;
    std::mt19937 random(417);
    for (int step = 0; step < 6000; ++step) {
        const auto operation = random() % 8;
        if (live.empty() || operation == 0) {
            live.push_back({world.create()});
        } else {
            const auto index = static_cast<std::size_t>(random()) % live.size();
            auto& expected = live[index];
            switch (operation) {
            case 1:
                dead.push_back(expected.id);
                REQUIRE(world.destroy(expected.id));
                live[index] = live.back();
                live.pop_back();
                break;
            case 2:
                expected.transform = true;
                expected.value = static_cast<float>(step);
                world.set_transform(expected.id).position.x = expected.value;
                break;
            case 3:
                REQUIRE(world.remove_transform(expected.id) == expected.transform);
                expected.transform = false;
                break;
            case 4:
                world.set_mesh(expected.id);
                expected.mesh = true;
                break;
            case 5:
                REQUIRE(world.remove_mesh(expected.id) == expected.mesh);
                expected.mesh = false;
                break;
            case 6:
                world.set_spin(expected.id);
                expected.spin = true;
                break;
            case 7:
                REQUIRE(world.remove_spin(expected.id) == expected.spin);
                expected.spin = false;
                break;
            default: break;
            }
        }
        if (step % 100 == 0) {
            REQUIRE(world.size() == live.size());
            std::size_t transforms = 0, meshes = 0, spins = 0;
            for (const auto& expected : live) {
                REQUIRE(world.alive(expected.id));
                REQUIRE((world.transform(expected.id) != nullptr) == expected.transform);
                REQUIRE((world.mesh(expected.id) != nullptr) == expected.mesh);
                REQUIRE((world.spin(expected.id) != nullptr) == expected.spin);
                if (expected.transform) {
                    REQUIRE(world.transform(expected.id)->position.x == expected.value);
                    ++transforms;
                }
                meshes += expected.mesh ? 1U : 0U;
                spins += expected.spin ? 1U : 0U;
            }
            REQUIRE(world.transforms().size() == transforms);
            REQUIRE(world.meshes().size() == meshes);
            REQUIRE(world.spins().size() == spins);
            for (const auto id : dead) {
                REQUIRE_FALSE(world.alive(id));
                REQUIRE(world.transform(id) == nullptr);
            }
        }
    }
    for (const auto& expected : live) {
        REQUIRE(world.destroy(expected.id));
    }
    REQUIRE(world.size() == 0);
    REQUIRE(world.transforms().empty());
    REQUIRE(world.meshes().empty());
    REQUIRE(world.spins().empty());
}
