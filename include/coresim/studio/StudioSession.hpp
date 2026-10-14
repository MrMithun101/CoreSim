#pragma once
#include <coresim/scene/DemoScene.hpp>
#include <coresim/physics/PhysicsSystem.hpp>
#include <coresim/core/FixedStepper.hpp>
#include <memory>
namespace coresim {
// Main-thread owner. All worker tasks join within a tick before controls can mutate state.
class StudioSession {
public:
    explicit StudioSession(SceneConfig config = {});
    const DemoScene& scene() const noexcept { return state_->scene; }
    DemoScene& scene() noexcept { return state_->scene; }
    bool paused() const noexcept { return paused_; }
    std::uint64_t ticks() const noexcept { return ticks_; }
    double dropped_seconds() const noexcept { return dropped_; }
    void select(SceneConfig config);
    void reset();
    void set_paused(bool paused);
    void request_step() noexcept;
    void advance(double seconds, Profiler* profiler = nullptr, JobSystem* jobs = nullptr);
private:
    struct State {
        explicit State(SceneConfig config) : scene(config), physics(scene.gravity()) {}
        DemoScene scene;
        PhysicsSystem physics;
    };
    void restart_clock() noexcept;
    std::unique_ptr<State> state_;
    FixedStepper clock_;
    std::uint64_t ticks_{};
    double dropped_{};
    bool paused_{};
    bool step_requested_{};
    unsigned int discard_frames_{};
};
}
