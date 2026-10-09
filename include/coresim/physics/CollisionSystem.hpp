#pragma once
#include <coresim/physics/Contact.hpp>
#include <vector>

namespace coresim {
class World;
class CollisionSystem {
public:
    void solve(World& world);
private:
    struct Constraint {
        Contact contact;
        float target_speed{};
        float accumulated_impulse{};
    };
    std::vector<Constraint> contacts_;
};
} // namespace coresim
