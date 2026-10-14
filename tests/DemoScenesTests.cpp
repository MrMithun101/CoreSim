#include <coresim/scene/DemoScene.hpp>
#include <coresim/physics/PhysicsSystem.hpp>
#include <coresim/jobs/JobSystem.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
using namespace coresim;
namespace {
void same_motion(const World& a,const World& b) {
    REQUIRE(a.rigid_bodies().size()==b.rigid_bodies().size());
    for (const auto& entry:a.rigid_bodies()) {
        const auto* other=b.rigid_body(entry.entity);
        REQUIRE(other);
        REQUIRE(entry.value.velocity==other->velocity);
        REQUIRE(entry.value.angular_velocity==other->angular_velocity);
        REQUIRE(a.transform(entry.entity)->position==b.transform(entry.entity)->position);
        REQUIRE(a.transform(entry.entity)->rotation==b.transform(entry.entity)->rotation);
        REQUIRE(entry.value.accumulated_force()==other->accumulated_force());
        REQUIRE(entry.value.accumulated_torque()==other->accumulated_torque());
    }
}
}
TEST_CASE("Named scenes validate defaults, counts and numeric input") {
    REQUIRE(scene_config("stress-tower").objects==256);
    REQUIRE(scene_config("sphere-rain").objects==2500);
    REQUIRE(scene_config("particle-storm").objects==25000);
    REQUIRE_THROWS_AS(scene_config("unknown"),std::invalid_argument);
    REQUIRE_THROWS_AS(scene_config("stress-tower",1001),std::invalid_argument);
    REQUIRE_THROWS_AS(scene_config("sphere-rain",25001),std::invalid_argument);
    REQUIRE_THROWS_AS(scene_config("particle-storm",100001),std::invalid_argument);
    REQUIRE_THROWS_AS(scene_config("collision-demo",10),std::invalid_argument);
    REQUIRE_THROWS_AS(DemoScene(SceneConfig{SceneKind::sphere_rain,0,1}),std::invalid_argument);
    REQUIRE_THROWS_AS(scene_name(static_cast<SceneKind>(99)),std::invalid_argument);
    for (auto invalid : {"-1","0","12x","","18446744073709551616"}) {
        REQUIRE_THROWS_AS(scene_integer(invalid,100000),std::invalid_argument);
    }
    REQUIRE(scene_integer("0",10,true)==0);
    REQUIRE(scene_integer("4294967295",std::numeric_limits<std::uint32_t>::max(),true)==4294967295ULL);
}
TEST_CASE("Seeded scene construction and reset restore exact motion") {
    for (auto name : {"stress-tower","sphere-rain","particle-storm"}) {
        DemoScene a(scene_config(name,64,7)), b(scene_config(name,64,7));
        same_motion(a.world(),b.world());
        REQUIRE(a.world().meshes().size()==(name==std::string_view("particle-storm") ? 64 : 65));
        PhysicsSystem physics(a.gravity());
        for (int tick=0;tick<60;++tick) { physics.step(a.world(),1.F/120); a.update(1.F/120); }
        for (const auto& entry:a.world().rigid_bodies()) {
            if (entry.value.inverse_mass()>0) {
                a.world().rigid_body(entry.entity)->add_force({1,2,3});
                a.world().rigid_body(entry.entity)->add_torque({3,2,1});
            }
        }
        a.reset_physics();
        same_motion(a.world(),b.world());
        DemoScene different(scene_config(name,64,8));
        bool changed=false;
        for (const auto& entry:b.world().rigid_bodies()) {
            changed |= entry.value.velocity!=different.world().rigid_body(entry.entity)->velocity;
            changed |= entry.value.angular_velocity!=different.world().rigid_body(entry.entity)->angular_velocity;
        }
        REQUIRE(changed);
        REQUIRE(std::isfinite(a.camera().position().x));
    }
}
TEST_CASE("Scene trajectories match serial and worker integration") {
    JobSystem jobs(4);
    for (auto name : {"stress-tower","sphere-rain","particle-storm"}) {
        DemoScene a(scene_config(name,64,19)), b(scene_config(name,64,19));
        PhysicsSystem serial(a.gravity()), parallel(b.gravity());
        CollisionStats sa,sb;
        for (int tick=0;tick<120;++tick) {
            serial.step(a.world(),1.F/120,&sa);
            parallel.step(b.world(),1.F/120,&sb,nullptr,&jobs,8);
            a.update(1.F/120); b.update(1.F/120);
            REQUIRE(sa.contacts==sb.contacts);
            REQUIRE(sa.candidate_pairs==sb.candidate_pairs);
        }
        same_motion(a.world(),b.world());
        for (const auto& entry:a.world().rigid_bodies()) {
            const auto y=a.world().transform(entry.entity)->position.y;
            REQUIRE(std::isfinite(y));
            if (a.config().kind!=SceneKind::particle_storm && entry.value.inverse_mass()>0) { REQUIRE(y>0.2F); }
        }
    }
}
TEST_CASE("Particle volume wraps without collisions and survives missing entities") {
    DemoScene scene(scene_config("particle-storm",1000));
    REQUIRE(scene.world().colliders().empty());
    REQUIRE(scene.gravity()==glm::vec3(0));
    const auto e=scene.world().rigid_bodies()[0].entity;
    const auto initial=scene.world().transform(e)->position;
    scene.world().transform(e)->position={1000,-1000,1000};
    scene.update(1.F/120);
    REQUIRE(glm::length(scene.world().transform(e)->position)<10.F);
    scene.reset_physics();
    REQUIRE(scene.world().transform(e)->position==initial);
    scene.world().destroy(e);
    scene.world().remove_transform(scene.world().rigid_bodies()[0].entity);
    REQUIRE_NOTHROW(scene.update(1.F/120));
    REQUIRE_NOTHROW(scene.reset_physics());
}
