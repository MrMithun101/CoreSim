#include <coresim/studio/StudioSession.hpp>
#include <coresim/jobs/JobSystem.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>
using namespace coresim;
TEST_CASE("Studio pause, step and resume exclude paused wall time") {
    StudioSession session(scene_config("sphere-rain",8,7));
    session.advance(FixedStepper::step_seconds);
    REQUIRE(session.ticks()==1);
    const auto entity=session.scene().world().rigid_bodies().front().entity;
    const auto position=session.scene().world().transform(entity)->position;
    session.set_paused(true);
    session.advance(100);
    REQUIRE(session.ticks()==1);
    REQUIRE(session.scene().world().transform(entity)->position==position);
    session.request_step();
    session.request_step(); // One command per frame, not a hidden backlog.
    session.advance(100);
    REQUIRE(session.ticks()==2);
    REQUIRE(session.scene().world().transform(entity)->position!=position);
    REQUIRE(session.paused());
    session.advance(100);
    REQUIRE(session.ticks()==2);
    session.set_paused(false);
    session.advance(100);
    REQUIRE(session.ticks()==2);
    REQUIRE(session.dropped_seconds()==0);
    session.request_step(); // Disabled while running.
    session.advance(FixedStepper::step_seconds);
    REQUIRE(session.ticks()==3);
}
TEST_CASE("Studio switches scenes transactionally and resets gravity and clocks") {
    StudioSession session(scene_config("sphere-rain",8,7));
    JobSystem jobs(2);
    for (const auto name:{"stress-tower","particle-storm","sphere-rain","collision-demo"}) {
        session.set_paused(true);
        session.request_step();
        const auto config=scene_config(name,std::string_view(name)=="collision-demo" ? 100 : 8,7);
        session.select(config);
        REQUIRE(session.paused());
        REQUIRE(session.ticks()==0);
        session.advance(100);
        REQUIRE(session.ticks()==0);
        const auto entity=session.scene().world().rigid_bodies().front().entity;
        const auto initial=*session.scene().world().transform(entity);
        const auto velocity=session.scene().world().rigid_body(entity)->velocity;
        session.request_step();
        session.advance(0,nullptr,&jobs);
        REQUIRE(session.ticks()==1);
        const auto actual=session.scene().world().rigid_body(entity)->velocity;
        if (config.kind==SceneKind::particle_storm) { REQUIRE(actual==velocity); }
        if (config.kind==SceneKind::sphere_rain) { REQUIRE(actual.y<velocity.y); }
        session.reset();
        REQUIRE(session.ticks()==0);
        REQUIRE(session.paused());
        REQUIRE(session.scene().world().transform(entity)->position==initial.position);
        REQUIRE(session.scene().world().rigid_body(entity)->velocity==velocity);
    }
    const auto* active=&session.scene();
    REQUIRE_THROWS(session.select(scene_config("stress-tower",1001)));
    REQUIRE(&session.scene()==active);
    REQUIRE_THROWS(session.select({SceneKind::sphere_rain,0,1}));
    REQUIRE(&session.scene()==active);
}
TEST_CASE("Studio reset clears residual time and pending steps") {
    StudioSession session(scene_config("sphere-rain",8));
    session.advance(FixedStepper::step_seconds*0.75);
    session.reset();
    session.advance(100);
    session.advance(100); // Includes time spent building/resetting the preceding frame.
    REQUIRE(session.ticks()==0);
    REQUIRE(session.dropped_seconds()==0);
    session.advance(FixedStepper::step_seconds*0.5);
    REQUIRE(session.ticks()==0);
    session.advance(FixedStepper::step_seconds*0.5);
    REQUIRE(session.ticks()==1);
    session.set_paused(true);
    session.request_step();
    session.reset();
    session.advance(0);
    REQUIRE(session.ticks()==0);
    REQUIRE_THROWS(session.advance(-1));
    REQUIRE_THROWS(session.advance(std::numeric_limits<double>::quiet_NaN()));
}
