#include <coresim/core/Application.hpp>
#include <iomanip>
#include <algorithm>
#include <iostream>
#include <sstream>

namespace coresim {
Application::Application(const std::filesystem::path& shader_directory, SceneConfig scene)
    : renderer_(shader_directory), session_(scene) { camera_=session_.scene().camera(); }

int Application::run(std::uint64_t frame_limit) {
    FrameTimer timer;
    while (!window_.should_close()) {
        timer.begin_frame(FrameTimer::Clock::now());
        {
            ScopedProfiler frame(&profiler_, ProfileSection::frame);
            window_.process_events();
            if (window_.should_close()) {
                break;
            }
            profiler_panel_.begin_frame();
            if (studio_panel_.draw(session_)) { camera_=session_.scene().camera(); }
            update(timer.stats());
            render();
        }
        timer.end_frame(FrameTimer::Clock::now());
        if (frame_limit > 0 && timer.stats().frame_count >= frame_limit) {
            window_.request_close();
        }
    }
    const auto& stats = timer.stats();
    std::cout << "CoreSim stopped cleanly: scene=" << scene_name(session_.scene().config().kind)
              << " objects=" << session_.scene().config().objects << " seed=" << session_.scene().config().seed << " frames=" << stats.frame_count
              << " elapsed_s=" << stats.elapsed_seconds << " average_fps=" << stats.fps
              << " last_frame_ms=" << stats.total_frame_seconds * 1000.0
              << " physics_ticks=" << session_.ticks()
              << " dropped_physics_s=" << session_.dropped_seconds() << '\n';
    for (std::size_t i = 0; i < profile_names.size(); ++i) {
        const auto& entry = profiler_.stats()[i];
        std::cout << "Profile " << profile_names[i] << ": calls=" << entry.count
                  << " total_ms=" << entry.total_ms << " avg_ms=" << entry.average_ms()
                  << " min_ms=" << entry.min_ms << " max_ms=" << entry.max_ms << '\n';
    }
    return 0;
}
void Application::update(const FrameStats& stats) {
    const auto input = window_.input(profiler_panel_.captures_keyboard(), profiler_panel_.captures_mouse());
    // Interactive motion must not leap after a debugger stop or a long OS stall.
    const auto dt = static_cast<float>(std::min(stats.delta_seconds, 0.1));
    camera_.look(input.look_x * 0.0025F, input.look_y * 0.0025F);
    camera_.move({input.right, input.up, input.forward}, dt);
    if (input.reset_physics) {
        session_.reset();
        camera_=session_.scene().camera();
    }
    session_.advance(stats.delta_seconds,&profiler_,&jobs_);
    if (stats.frame_count > 0 && stats.elapsed_seconds >= next_title_update_) {
        std::ostringstream title;
        title << std::fixed << std::setprecision(2) << "CoreSim Studio | " << scene_name(session_.scene().config().kind) << " | " << session_.scene().world().size()
              << " entities | R reset | sim "
              << static_cast<double>(session_.ticks()) * FixedStepper::step_seconds
              << " s | dropped " << session_.dropped_seconds() << " s | FPS " << stats.fps
              << " | dt " << stats.delta_seconds * 1000.0 << " ms | frame "
              << stats.total_frame_seconds * 1000.0 << " ms";
        window_.set_title(title.str());
        next_title_update_ = stats.elapsed_seconds + 0.5;
    }
}
void Application::render() {
    {
        ScopedProfiler rendering(&profiler_, ProfileSection::rendering);
        const auto [width, height] = window_.framebuffer_size();
        if (width > 0 && height > 0) {
            const float aspect = static_cast<float>(width) / static_cast<float>(height);
            renderer_.draw(width, height, camera_.projection(aspect) * camera_.view(), session_.scene().world());
        }
        profiler_panel_.draw(profiler_,true);
        profiler_panel_.render();
    }
    ScopedProfiler presentation(&profiler_, ProfileSection::presentation);
    window_.present();
}
} // namespace coresim
