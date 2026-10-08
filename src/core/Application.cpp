#include <coresim/core/Application.hpp>
#include <iomanip>
#include <cmath>
#include <numbers>
#include <iostream>
#include <sstream>

namespace coresim {
Application::Application(const std::filesystem::path& shader_directory)
    : renderer_(shader_directory) {}

int Application::run(std::uint64_t frame_limit) {
    FrameTimer timer;
    while (!window_.should_close()) {
        timer.begin_frame(FrameTimer::Clock::now());
        window_.process_events();
        if (window_.should_close()) {
            break;
        }
        update(timer.stats());
        render();
        timer.end_frame(FrameTimer::Clock::now());
        if (frame_limit > 0 && timer.stats().frame_count >= frame_limit) {
            window_.request_close();
        }
    }
    const auto& stats = timer.stats();
    std::cout << "CoreSim stopped cleanly: frames=" << stats.frame_count
              << " elapsed_s=" << stats.elapsed_seconds << " average_fps=" << stats.fps
              << " last_frame_ms=" << stats.total_frame_seconds * 1000.0 << '\n';
    return 0;
}
void Application::update(const FrameStats& stats) {
    angle_radians_ = std::fmod(angle_radians_ + stats.delta_seconds * 0.7,
                               2.0 * std::numbers::pi);
    if (stats.frame_count > 0 && stats.elapsed_seconds >= next_title_update_) {
        std::ostringstream title;
        title << std::fixed << std::setprecision(2) << "CoreSim | avg FPS " << stats.fps
              << " | dt " << stats.delta_seconds * 1000.0 << " ms | frame "
              << stats.total_frame_seconds * 1000.0 << " ms";
        window_.set_title(title.str());
        next_title_update_ = stats.elapsed_seconds + 0.5;
    }
}
void Application::render() {
    const auto [width, height] = window_.framebuffer_size();
    renderer_.draw(width, height, angle_radians_);
    window_.present();
}
} // namespace coresim
