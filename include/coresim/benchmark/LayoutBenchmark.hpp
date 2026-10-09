#pragma once
#include <coresim/scene/World.hpp>
#include <array>
#include <cstddef>
#include <vector>
namespace coresim {
enum class IntegrationLayout { lookup, dense, aos, soa };
struct LayoutTiming { double gather_ms{}, kernel_ms{}, scatter_ms{}, total_ms{}; };
// Experimental per-tick packing; buffers retain capacity. Not the production storage owner.
class LayoutExperiment {
public:
    LayoutTiming step(World& world, IntegrationLayout layout, float dt);
    static constexpr glm::vec3 gravity{0,-9.81F,0};
    struct PackedBody { glm::vec3 position, velocity, acceleration, force; float inverse_mass; };
private:
    struct Target { RigidBody* body; Transform* transform; };
    std::vector<Target> targets_;
    std::vector<PackedBody> aos_;
    std::array<std::vector<float>, 13> soa_;
};
void make_layout_world(World& world, std::size_t bodies, bool shuffled);
double layout_checksum(const World& world);
}
