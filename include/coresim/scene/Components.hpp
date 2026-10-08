#pragma once

#include <glm/glm.hpp>

namespace coresim {
// A mesh reference carries no GPU ownership; the renderer owns shared geometry.
enum class MeshKind { cube };
struct MeshComponent {
    MeshKind kind{MeshKind::cube};
};

// Demo animation only, not rigid-body angular dynamics. Axis must be normalized.
struct SpinComponent {
    glm::vec3 axis{0.0F, 1.0F, 0.0F};
    float radians_per_second{};
};
} // namespace coresim
