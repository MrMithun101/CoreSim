#pragma once
#include <coresim/core/FrameTimer.hpp>
#include <coresim/core/Window.hpp>
#include <cstdint>
#include <coresim/renderer/Renderer.hpp>
#include <coresim/scene/Camera.hpp>
#include <coresim/scene/DemoScene.hpp>

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
    double next_title_update_{};
};
} // namespace coresim
