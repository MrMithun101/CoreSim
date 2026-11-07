#include <coresim/studio/StudioSession.hpp>
#include <coresim/jobs/JobSystem.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>
using namespace coresim;
TEST_CASE("Studio pause, step and resume exclude paused wall time") {
    REQUIRE(scene_config("sandbox").kind==SceneKind::sandbox);
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

TEST_CASE("Sandbox authors supported physical shapes and restores the experiment after stepping") {
    StudioSession session(scene_config("sandbox"));
    REQUIRE(session.paused());auto& e=*session.scene().editor();
    REQUIRE(e.items().size()==3);
    const Camera camera=session.scene().camera();
    REQUIRE(glm::distance(camera_ray(camera,{0,0},1.5F).direction,camera.direction())<.0001F);
    for(auto shape:{Primitive::cube,Primitive::sphere,Primitive::platform,Primitive::ramp,Primitive::wall,Primitive::plane}){
        auto spec=primitive_spec(shape,{4,3,-4});
        auto id=e.create(spec);
        REQUIRE(e.selected()->entity==id);REQUIRE(e.world().collider(id)!=nullptr);
        spec.pose.scale={2,1,3};spec.gravity=false;e.change(spec);
        if(shape==Primitive::sphere)REQUIRE(std::get<SphereCollider>(*e.world().collider(id)).radius==2);
        else if(shape==Primitive::plane)REQUIRE(e.world().rigid_body(id)->inverse_mass()==0);
        else REQUIRE(std::get<BoxCollider>(*e.world().collider(id)).half_extents==spec.pose.scale);
        REQUIRE_FALSE(e.world().rigid_body(id)->gravity_enabled);
        e.duplicate();auto copy=e.selected()->entity;REQUIRE(copy!=id);
        e.erase();REQUIRE_FALSE(e.world().alive(copy));REQUIRE(e.selected()==nullptr);
        e.select(id);e.erase();
    }
    auto o=primitive_spec(Primitive::sphere,{0,5,0});o.velocity={1,0,0};
    auto id=e.create(o);e.pick({{0,5,5},{0,0,-1}});REQUIRE(e.selected()->entity==id);
    auto bad=o;bad.pose.scale.x=-1;REQUIRE_THROWS(e.change(bad));REQUIRE(e.world().transform(id)->scale==o.pose.scale);
    session.request_step();session.advance(0);REQUIRE(session.ticks()==1);
    REQUIRE(e.world().transform(id)->position!=o.pose.position);REQUIRE_THROWS(e.erase());
    session.reset();REQUIRE(e.building());REQUIRE(session.paused());REQUIRE(session.ticks()==0);
    REQUIRE(e.world().transform(id)->position==o.pose.position);REQUIRE(e.world().rigid_body(id)->velocity==o.velocity);
    e.pick({{0,100,0},{0,1,0}});REQUIRE(e.selected()==nullptr);
}

TEST_CASE("Supported sandbox bodies settle, wake under force, and fall if support is removed") {
    StudioSession session(scene_config("sandbox"));auto& e=*session.scene().editor();
    auto bottom=e.create(primitive_spec(Primitive::cube,{4,.65F,0}));
    auto top=e.create(primitive_spec(Primitive::cube,{4,1.95F,0}));
    auto floor=e.items()[0].entity;
    session.set_paused(false);
    for(int i=0;i<720;++i)session.advance(FixedStepper::step_seconds);
    REQUIRE(e.world().rigid_body(bottom)->sleeping());
    REQUIRE(e.world().rigid_body(top)->sleeping());
    REQUIRE(e.world().transform(bottom)->position.y>.62F);
    e.world().rigid_body(top)->add_force({240,0,0});
    session.advance(FixedStepper::step_seconds);
    REQUIRE_FALSE(e.world().rigid_body(top)->sleeping());
    REQUIRE(e.world().rigid_body(top)->velocity.x>0);
    REQUIRE(e.world().destroy(floor));
    for(int i=0;i<20;++i)session.advance(FixedStepper::step_seconds);
    REQUIRE_FALSE(e.world().rigid_body(bottom)->sleeping());
    REQUIRE(e.world().rigid_body(bottom)->velocity.y<0);
}
