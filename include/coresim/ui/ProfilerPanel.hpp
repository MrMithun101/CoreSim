#pragma once
#include <coresim/core/Profiler.hpp>

namespace coresim {
class Window;
// Owns one ImGui context and its backends; destroy before the OpenGL window.
class ProfilerPanel {
public:
    explicit ProfilerPanel(Window& window);
    ~ProfilerPanel();
    ProfilerPanel(const ProfilerPanel&) = delete;
    ProfilerPanel& operator=(const ProfilerPanel&) = delete;
    void begin_frame();
    void draw(Profiler& profiler);
    void render();
    [[nodiscard]] bool captures_keyboard() const;
    [[nodiscard]] bool captures_mouse() const;
private:
    std::array<ProfileStats, profile_names.size()> snapshot_{};
    double next_refresh_{};
};
} // namespace coresim
