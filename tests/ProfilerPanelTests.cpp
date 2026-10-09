#include <coresim/core/Window.hpp>
#include <coresim/ui/ProfilerPanel.hpp>
#include <imgui.h>
#include <glad/gl.h>
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <fstream>
#include <vector>

using namespace coresim;
TEST_CASE("Profiler panel renders accumulated statistics and releases its context") {
    GlfwRuntime runtime;
    Window window(runtime, 800, 400, false);
    GLuint framebuffer{}, texture{};
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 800, 400, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    REQUIRE(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    Profiler profiler;
    for (std::size_t i = 0; i < profile_names.size(); ++i) {
        profiler.record(static_cast<ProfileSection>(i), static_cast<double>(i + 1) * 0.125);
        profiler.record(static_cast<ProfileSection>(i), static_cast<double>(i + 1) * 0.25);
    }
    for (int lifecycle = 0; lifecycle < 2; ++lifecycle) {
        {
            ProfilerPanel panel(window);
            for (int frame = 0; frame < 3; ++frame) {
                window.process_events();
                panel.begin_frame();
                ImGui::GetIO().DisplaySize = {800,400};
                ImGui::GetIO().DisplayFramebufferScale = {1,1};
                glViewport(0,0,800,400);
                glClearColor(0.04F,0.05F,0.08F,1);
                glClear(GL_COLOR_BUFFER_BIT);
                panel.draw(profiler);
                panel.render();
                REQUIRE(ImGui::GetDrawData()->TotalVtxCount > 0);
                REQUIRE(glGetError() == GL_NO_ERROR);
            }
            std::vector<unsigned char> pixels(800 * 400 * 4);
            glReadPixels(0,0,800,400,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
            std::size_t bright = 0;
            for (std::size_t i = 0; i < pixels.size(); i += 4) {
                if (pixels[i] > 100 && pixels[i+1] > 100 && pixels[i+2] > 100) { ++bright; }
            }
            REQUIRE(bright > 2000); // Visible text/table, not just a cleared background.
            if (const auto* path = std::getenv("CORESIM_PROFILER_IMAGE"); path && lifecycle == 0) {
                std::ofstream image(path, std::ios::binary);
                image << "P6\n800 400\n255\n";
                for (int y = 399; y >= 0; --y) {
                    for (int x = 0; x < 800; ++x) {
                        const auto offset = static_cast<std::size_t>((y * 800 + x) * 4);
                        image.write(reinterpret_cast<const char*>(pixels.data() + offset), 3);
                    }
                }
                REQUIRE(image.good());
            }
            if (lifecycle == 0) {
                // Fixed default layout, verified against the framebuffer above.
                const auto click = [&](float x, float y) {
                    for (bool down : {true, false}) {
                        ImGui::GetIO().AddMousePosEvent(x, y);
                        ImGui::GetIO().AddMouseButtonEvent(0, down);
                        panel.begin_frame();
                        ImGui::GetIO().DisplaySize = {800,400};
                        ImGui::GetIO().DisplayFramebufferScale = {1,1};
                        panel.draw(profiler);
                        panel.render();
                    }
                };
                click(29, 48);
                REQUIRE_FALSE(profiler.enabled());
                profiler.record(ProfileSection::frame, 99);
                REQUIRE(profiler.stats()[0].count == 2);
                click(29, 48);
                REQUIRE(profiler.enabled());
                click(212, 48);
                for (const auto& entry : profiler.stats()) { REQUIRE(entry.count == 0); }
                REQUIRE(glGetError() == GL_NO_ERROR);
            }
        }
        REQUIRE(ImGui::GetCurrentContext() == nullptr);
    }
    glDeleteTextures(1, &texture);
    glDeleteFramebuffers(1, &framebuffer);
}
