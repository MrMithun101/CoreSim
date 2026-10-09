#pragma once
#include <coresim/core/InputState.hpp>
#include <memory>
#include <string>
#include <utility>

struct GLFWwindow;
namespace coresim {
// One runtime per application; GLFW operations remain on the main thread.
class GlfwRuntime {
public:
    GlfwRuntime();
    ~GlfwRuntime();
    GlfwRuntime(const GlfwRuntime&) = delete;
    GlfwRuntime& operator=(const GlfwRuntime&) = delete;
    GlfwRuntime(GlfwRuntime&&) = delete;
    GlfwRuntime& operator=(GlfwRuntime&&) = delete;
};
class Window {
public:
    explicit Window(const GlfwRuntime& runtime, int width = 1280, int height = 720,
                    bool visible = true);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;
    void process_events();
    [[nodiscard]] InputState input(bool keyboard_captured = false, bool mouse_captured = false);
    [[nodiscard]] GLFWwindow* native_handle() const noexcept { return handle_.get(); }
    [[nodiscard]] bool should_close() const;
    void request_close();
    [[nodiscard]] std::pair<int, int> framebuffer_size() const;
    void present();
    void set_title(const std::string& title);
private:
    struct Deleter {
        void operator()(GLFWwindow* window) const noexcept;
    };
    std::unique_ptr<GLFWwindow, Deleter> handle_;
    bool mouse_captured_{};
    bool reset_was_down_{};
    double cursor_x_{};
    double cursor_y_{};
};
} // namespace coresim
