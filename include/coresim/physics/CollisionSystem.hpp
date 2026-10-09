#pragma once
#include <coresim/physics/Contact.hpp>
#include <coresim/physics/SpatialHash.hpp>
#include <vector>
#include <cstdint>

namespace coresim {
class World;
enum class BroadPhase { naive, spatial_hash };
struct CollisionStats {
    std::uint64_t candidate_pairs{}, collision_checks{}, contacts{}, correction_checks{};
    double broad_phase_ms{}, narrow_phase_ms{}, solver_ms{};
    double hash_build_ms{}, query_ms{}, total_collision_ms{}, pair_reduction_percent{};
};
class CollisionSystem {
public:
    explicit CollisionSystem(BroadPhase mode = BroadPhase::spatial_hash, float cell_size = 3.0F)
        : mode_(mode), spatial_(cell_size) {}
    void solve(World& world, CollisionStats* stats = nullptr);
private:
    struct Constraint {
        Contact contact;
        float target_speed{};
        float accumulated_impulse{};
    };
    std::vector<Constraint> contacts_;
    BroadPhase mode_;
    SpatialHash spatial_;
};
} // namespace coresim
