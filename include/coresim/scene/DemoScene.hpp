#pragma once

#include <coresim/scene/Transform.hpp>
#include <array>
#include <span>

namespace coresim {
class DemoScene {
public:
    DemoScene();
    void update(float delta_seconds);
    [[nodiscard]] std::span<const Transform> transforms() const noexcept { return transforms_; }
private:
    std::array<Transform, 64> transforms_{};
};
} // namespace coresim
