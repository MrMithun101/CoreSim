#include <coresim/studio/StudioSession.hpp>
#include <cmath>
#include <stdexcept>
namespace coresim {
StudioSession::StudioSession(SceneConfig config) : state_(std::make_unique<State>(config)) {}
void StudioSession::restart_clock() noexcept {
    clock_.reset();
    ticks_=0;
    dropped_=0;
    step_requested_=false;
    discard_frames_=2;
}
void StudioSession::select(SceneConfig config) {
    // Construct first: validation/allocation failures leave the active scene intact.
    auto replacement=std::make_unique<State>(config);
    state_.swap(replacement);
    restart_clock();
}
void StudioSession::reset() {
    state_->scene.reset_physics();
    restart_clock();
}
void StudioSession::set_paused(bool paused) {
    if (paused_==paused) { return; }
    paused_=paused;
    clock_.reset();
    step_requested_=false;
    discard_frames_=1;
}
void StudioSession::request_step() noexcept {
    if (paused_) { step_requested_=true; }
}
void StudioSession::advance(double seconds, Profiler* profiler, JobSystem* jobs) {
    if (!std::isfinite(seconds) || seconds<0) { throw std::invalid_argument("Invalid Studio frame duration"); }
    const auto tick=[&](float step) {
        state_->physics.step(state_->scene.world(),step,nullptr,profiler,jobs);
        state_->scene.update(step);
        ++ticks_;
    };
    const bool discard=discard_frames_>0;
    if (discard) { --discard_frames_; }
    if (paused_) {
        if (std::exchange(step_requested_,false)) { tick(static_cast<float>(FixedStepper::step_seconds)); }
        return;
    }
    if (discard) { return; } // Rebase both the control frame and the following frame containing rebuild work.
    const auto result=clock_.advance(seconds,tick);
    dropped_+=result.dropped_seconds;
}
}
