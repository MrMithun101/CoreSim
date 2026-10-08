#include <coresim/core/Application.hpp>
#include <iomanip>
#include <algorithm>
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
    const auto input = window_.input();
    // Interactive motion must not leap after a debugger stop or a long OS stall.
    const auto dt = static_cast<float>(std::min(stats.delta_seconds, 0.1));
    camera_.look(input.look_x * 0.0025F, input.look_y * 0.0025F);
    camera_.move({input.right, input.up, input.forward}, dt);
    scene_.update(dt);
    if (stats.frame_count > 0 && stats.elapsed_seconds >= next_title_update_) {
        std::ostringstream title;
        title << std::fixed << std::setprecision(2) << "CoreSim | " << scene_.world().size()
              << " entities | WASD/QE move, RMB look | avg FPS " << stats.fps
              << " | dt " << stats.delta_seconds * 1000.0 << " ms | frame "
              << stats.total_frame_seconds * 1000.0 << " ms";
        window_.set_title(title.str());
        next_title_update_ = stats.elapsed_seconds + 0.5;
    }
}
void Application::render() {
    const auto [width, height] = window_.framebuffer_size();
    if (width > 0 && height > 0) {
        const float aspect = static_cast<float>(width) / static_cast<float>(height);
        renderer_.draw(width, height, camera_.projection(aspect) * camera_.view(), scene_.world());
    }
    window_.present();
}
} // namespace coresim
