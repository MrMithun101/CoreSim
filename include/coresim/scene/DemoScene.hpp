#pragma once

#include <coresim/scene/World.hpp>

namespace coresim {
class DemoScene {
public:
    DemoScene();
    void update(float delta_seconds);
    [[nodiscard]] World& world() noexcept { return world_; }
    [[nodiscard]] const World& world() const noexcept { return world_; }
private:
    World world_;
};
} // namespace coresim
