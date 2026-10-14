#include <coresim/scene/DemoScene.hpp>
#include <coresim/physics/PhysicsSystem.hpp>
#include <coresim/jobs/JobSystem.hpp>
#include <coresim/core/FixedStepper.hpp>
#include <bit>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <string_view>

int main(int argc,char** argv) {
    try {
        std::string_view name="collision-demo";
        std::size_t objects=0, steps=120, workers=0;
        std::uint32_t seed=1;
        std::set<std::string_view> seen;
        for (int i=1;i<argc;++i) {
            const std::string_view key=argv[i];
            if (key=="--help") {
                std::cout<<"coresim_scene_runner --scene collision-demo|stress-tower|sphere-rain|particle-storm [--objects count] [--seed uint32] [--steps 1..10000] [--workers 0..8]\n";
                return 0;
            }
            if (!seen.insert(key).second || i+1>=argc) { throw std::invalid_argument("Duplicate or missing option"); }
            const std::string_view value=argv[++i];
            if (key=="--scene") { name=value; }
            else if (key=="--objects") { objects=static_cast<std::size_t>(coresim::scene_integer(value,100000)); }
            else if (key=="--steps") { steps=static_cast<std::size_t>(coresim::scene_integer(value,10000)); }
            else if (key=="--workers") { workers=static_cast<std::size_t>(coresim::scene_integer(value,8,true)); }
            else if (key=="--seed") { seed=static_cast<std::uint32_t>(coresim::scene_integer(value,std::numeric_limits<std::uint32_t>::max(),true)); }
            else { throw std::invalid_argument("Unknown scene option"); }
        }
        coresim::DemoScene scene(coresim::scene_config(name,objects,seed));
        coresim::PhysicsSystem physics(scene.gravity());
        std::unique_ptr<coresim::JobSystem> jobs;
        if (workers) { jobs=std::make_unique<coresim::JobSystem>(workers); }
        std::cout<<"schema,scene,objects,seed,workers,tick,simulation_s,candidate_pairs,contacts,min_center_y,max_speed,motion_hash\n"<<std::setprecision(12);
        const auto record=[&](std::size_t tick,const coresim::CollisionStats& stats) {
            float minimum=std::numeric_limits<float>::max(), maximum=0;
            std::uint64_t hash=14695981039346656037ULL;
            const auto mix=[&](float value) {
                if (!std::isfinite(value)) { throw std::runtime_error("Non-finite scene state"); }
                const auto bits=std::bit_cast<std::uint32_t>(value);
                for (unsigned int shift=0;shift<32;shift+=8) { hash^=(bits>>shift)&255U; hash*=1099511628211ULL; }
            };
            for (const auto& entry:scene.world().rigid_bodies()) {
                if (entry.value.inverse_mass()==0) { continue; }
                const auto& t=*scene.world().transform(entry.entity);
                minimum=std::min(minimum,t.position.y);
                maximum=std::max(maximum,glm::length(entry.value.velocity));
                for (int axis=0;axis<3;++axis) { mix(t.position[axis]); mix(entry.value.velocity[axis]); mix(entry.value.angular_velocity[axis]); }
                for (int axis=0;axis<4;++axis) { mix(t.rotation[axis]); }
            }
            if (!std::isfinite(maximum)) { throw std::runtime_error("Non-finite scene speed"); }
            std::cout<<"1,"<<name<<','<<scene.config().objects<<','<<seed<<','<<workers<<','<<tick<<','
                     <<static_cast<double>(tick)*coresim::FixedStepper::step_seconds<<','<<stats.candidate_pairs<<','<<stats.contacts<<','<<minimum<<','<<maximum<<','<<hash<<'\n';
        };
        record(0,{});
        for (std::size_t tick=1;tick<=steps;++tick) {
            coresim::CollisionStats stats;
            physics.step(scene.world(),static_cast<float>(coresim::FixedStepper::step_seconds),&stats,nullptr,jobs.get());
            scene.update(static_cast<float>(coresim::FixedStepper::step_seconds));
            record(tick,stats);
        }
    } catch (const std::exception& e) { std::cerr<<"CoreSim scene: "<<e.what()<<'\n'; return 1; }
}
