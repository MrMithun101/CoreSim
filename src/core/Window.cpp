#include <coresim/core/Window.hpp>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <stdexcept>

namespace coresim {
GlfwRuntime::GlfwRuntime() {
    glfwSetErrorCallback([](int code, const char* description) {
        std::fprintf(stderr, "GLFW error %d: %s\n", code, description);
    });
    if (glfwInit() != GLFW_TRUE) {
        glfwSetErrorCallback(nullptr);
        throw std::runtime_error("GLFW initialization failed; check display availability");
    }
}
GlfwRuntime::~GlfwRuntime() {
    glfwTerminate();
    glfwSetErrorCallback(nullptr);
}
void Window::Deleter::operator()(GLFWwindow* window) const noexcept {
    glfwDestroyWindow(window);
}
Window::Window(const GlfwRuntime&, int width, int height, bool visible) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Window dimensions must be positive");
    }
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_VISIBLE, visible ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    handle_.reset(glfwCreateWindow(width, height, "CoreSim | WASD move, hold RMB to look", nullptr, nullptr));
    if (!handle_) {
        throw std::runtime_error("Could not create CoreSim OpenGL 3.3 window");
    }
    glfwMakeContextCurrent(handle_.get());
    if (gladLoadGL(glfwGetProcAddress) == 0 || !GLAD_GL_VERSION_3_3) {
        throw std::runtime_error("Could not load OpenGL 3.3 entry points");
    }
    glfwSwapInterval(1);
}
Window::~Window() = default;
void Window::process_events() {
    glfwPollEvents();
    if (glfwGetKey(handle_.get(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        request_close();
    }
}
InputState Window::input() {
    InputState state;
    const bool focused = glfwGetWindowAttrib(handle_.get(), GLFW_FOCUSED) == GLFW_TRUE;
    const bool capture = focused && glfwGetMouseButton(handle_.get(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    const bool changed = capture != mouse_captured_;
    if (changed) {
        glfwSetInputMode(handle_.get(), GLFW_CURSOR, capture ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        mouse_captured_ = capture;
    }
    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(handle_.get(), &x, &y);
    if (capture && !changed) {
        state.look_x = static_cast<float>(x - cursor_x_);
        state.look_y = static_cast<float>(cursor_y_ - y);
    }
    // Rebase after capture/focus transitions so the first sample cannot jump.
    cursor_x_ = x;
    cursor_y_ = y;
    if (focused) {
        const auto pressed = [this](int key) {
            return glfwGetKey(handle_.get(), key) == GLFW_PRESS ? 1.0F : 0.0F;
        };
        state.right = pressed(GLFW_KEY_D) - pressed(GLFW_KEY_A);
        state.forward = pressed(GLFW_KEY_W) - pressed(GLFW_KEY_S);
        state.up = pressed(GLFW_KEY_E) - pressed(GLFW_KEY_Q);
    }
    return state;
}
bool Window::should_close() const { return glfwWindowShouldClose(handle_.get()) == GLFW_TRUE; }
void Window::request_close() { glfwSetWindowShouldClose(handle_.get(), GLFW_TRUE); }
std::pair<int, int> Window::framebuffer_size() const {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(handle_.get(), &width, &height);
    return {width, height};
}
void Window::present() { glfwSwapBuffers(handle_.get()); }
void Window::set_title(const std::string& title) { glfwSetWindowTitle(handle_.get(), title.c_str()); }
} // namespace coresim
