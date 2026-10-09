#pragma once
#include <coresim/physics/Contact.hpp>
#include <vector>
#include <cstdint>

namespace coresim {
class World;
struct CollisionStats {
    std::uint64_t candidate_pairs{}, collision_checks{}, contacts{}, correction_checks{};
    double broad_phase_ms{}, narrow_phase_ms{}, solver_ms{};
};
class CollisionSystem {
public:
    void solve(World& world, CollisionStats* stats = nullptr);
private:
    struct Constraint {
        Contact contact;
        float target_speed{};
        float accumulated_impulse{};
    };
    std::vector<Constraint> contacts_;
};
} // namespace coresim
