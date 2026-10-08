#pragma once
#include <coresim/core/FrameTimer.hpp>
#include <coresim/core/Window.hpp>
#include <cstdint>
#include <coresim/renderer/Renderer.hpp>
#include <coresim/scene/Camera.hpp>
#include <coresim/scene/DemoScene.hpp>
#include <coresim/core/FixedStepper.hpp>
#include <coresim/physics/PhysicsSystem.hpp>

namespace coresim {
class Application {
public:
    explicit Application(const std::filesystem::path& shader_directory);
    // Zero runs interactively; positive values bound display smoke tests.
    int run(std::uint64_t frame_limit = 0);
private:
    void update(const FrameStats& stats);
    void render();
    // Reverse destruction order closes the context before terminating GLFW.
    GlfwRuntime runtime_;
    Window window_{runtime_};
    Renderer renderer_;
    Camera camera_{{16.0F, 18.0F, 27.0F}, -2.106F, -0.52F};
    DemoScene scene_;
    PhysicsSystem physics_;
    FixedStepper physics_clock_;
    std::uint64_t physics_ticks_{};
    double dropped_physics_seconds_{};
    double next_title_update_{};
};
} // namespace coresim
