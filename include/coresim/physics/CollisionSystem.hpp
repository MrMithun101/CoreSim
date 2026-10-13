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
        glm::vec3 offset_a{}, offset_b{}, angular_a{}, angular_b{};
        float effective_inverse_mass{};
        glm::mat3 inverse_a{}, inverse_b{};
        glm::vec3 tangent_a{}, tangent_b{};
        glm::mat2 tangent_mass{};
        glm::vec2 tangent_impulse{};
        float friction{};
    };
    std::vector<Constraint> contacts_;
    std::vector<std::array<Entity,2>> contact_pairs_;
    BroadPhase mode_;
    SpatialHash spatial_;
};
} // namespace coresim
