#include <coresim/studio/StudioSession.hpp>
#include <cmath>
#include <chrono>
#include <coresim/studio/SceneFile.hpp>
#include <stdexcept>
namespace coresim {
StudioSession::StudioSession(SceneConfig config) : state_(std::make_unique<State>(config)), paused_(state_->scene.editor()!=nullptr) {}
void StudioSession::restart_clock() noexcept {
    clock_.reset();
    ticks_=0;collision_={};physics_ms_=0;
    dropped_=0;
    step_requested_=false;
    discard_frames_=2;
}
void StudioSession::select(SceneConfig config) {
    // Construct first: validation/allocation failures leave the active scene intact.
    auto replacement=std::make_unique<State>(config);
    state_.swap(replacement);
    if(state_->scene.editor()) paused_=true;
    restart_clock();
}
void StudioSession::load(const SceneDocument& doc){
    if(doc.kind!=SceneKind::sandbox && doc.kind!=SceneKind::structural_lab)throw std::invalid_argument("Not an editor document");
    auto next=std::make_unique<State>(scene_config(scene_name(doc.kind)));
    next->scene.editor()->replace_objects(doc.objects,doc.projectile,doc.launch);
    state_.swap(next);paused_=true;restart_clock();
}
void StudioSession::reset() {
    state_->scene.reset_physics();
    if(state_->scene.editor())paused_=true;
    restart_clock();
}
void StudioSession::set_paused(bool paused) {
    if (paused_==paused) { return; }
    if(!paused && state_->scene.editor())state_->scene.editor()->simulate();
    paused_=paused;
    clock_.reset();
    step_requested_=false;
    discard_frames_=1;
}
void StudioSession::request_step() noexcept {
    if (paused_) { if(state_->scene.editor())state_->scene.editor()->simulate(); step_requested_=true; }
}
void StudioSession::advance(double seconds, Profiler* profiler, JobSystem* jobs) {
    if (!std::isfinite(seconds) || seconds<0) { throw std::invalid_argument("Invalid Studio frame duration"); }
    const auto tick=[&](float step) {
        using Clock=std::chrono::steady_clock;
        const auto begin=collect_?Clock::now():Clock::time_point{};
        state_->physics.step(state_->scene.world(),step,collect_?&collision_:nullptr,profiler,jobs);
        if(collect_)physics_ms_=std::chrono::duration<double,std::milli>(Clock::now()-begin).count();
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
