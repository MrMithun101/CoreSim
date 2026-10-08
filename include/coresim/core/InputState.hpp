#pragma once

namespace coresim {
// A per-frame value snapshot; GLFW key codes never enter the camera or renderer.
struct InputState {
    bool reset_physics{};
    float right{};
    float up{};
    float forward{};
    float look_x{};
    float look_y{};
};
} // namespace coresim
