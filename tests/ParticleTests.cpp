#include <coresim/particles/ParticleSimulation.hpp>
#include <coresim/particles/ParticleRunner.hpp>
#include <coresim/jobs/JobSystem.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
using namespace coresim;
using namespace coresim::particles;
TEST_CASE("Particle initialization is seeded, stable and bounded") {
    const auto a=initial_particles(1000,7), b=initial_particles(1000,7), c=initial_particles(1000,8);
    REQUIRE(a==b);
    REQUIRE(a!=c);
    REQUIRE(std::bit_cast<std::uint32_t>(a[0].x)==0xc085be86U);
    REQUIRE(std::bit_cast<std::uint32_t>(a[0].y)==0x40d3b562U);
    REQUIRE(std::bit_cast<std::uint32_t>(a[0].z)==0x3fe66200U);
    REQUIRE(std::bit_cast<std::uint32_t>(a[0].vx)==0x3f5a9d50U);
    REQUIRE(std::bit_cast<std::uint32_t>(a[0].vy)==0xbf66bcc2U);
    REQUIRE(std::bit_cast<std::uint32_t>(a[0].vz)==0x3f362942U);
    REQUIRE(std::all_of(a.begin(),a.end(),[](const Particle& p) {
        return p.x>=-8 && p.x<8 && p.y>=-8 && p.y<8 && p.z>=-8 && p.z<8 &&
               p.vx>=-1 && p.vx<1 && p.vy>=-1 && p.vy<1 && p.vz>=-1 && p.vz<1;
    }));
    REQUIRE(initial_particles(0,0).empty());
    REQUIRE_THROWS_AS(initial_particles(maximum_particles+1,1),std::invalid_argument);
}
TEST_CASE("Particle integration matches independent semi-implicit Euler equations") {
    const std::vector<Particle> initial{{1,2,3,4,5,6},{-3,0,7,-1,2,-4}};
    const Acceleration acceleration{2,-3,4};
    ParticleSimulation simulation(initial,acceleration);
    constexpr int ticks=120;
    for (int i=0;i<ticks;++i) { REQUIRE(simulation.step_serial().worker_tasks==0); }
    const double dt=timestep;
    for (std::size_t i=0;i<initial.size();++i) {
        const auto& actual=simulation.state()[i];
        const auto& p=initial[i];
        const double expected_x=p.x+p.vx*ticks*dt+acceleration[0]*dt*dt*ticks*(ticks+1)/2;
        const double expected_y=p.y+p.vy*ticks*dt+acceleration[1]*dt*dt*ticks*(ticks+1)/2;
        const double expected_z=p.z+p.vz*ticks*dt+acceleration[2]*dt*dt*ticks*(ticks+1)/2;
        REQUIRE(std::abs(actual.x-expected_x)<0.0002);
        REQUIRE(std::abs(actual.y-expected_y)<0.0002);
        REQUIRE(std::abs(actual.z-expected_z)<0.0002);
        REQUIRE(std::abs(actual.vx-(p.vx+acceleration[0]*ticks*dt))<0.0002);
        REQUIRE(std::abs(actual.vy-(p.vy+acceleration[1]*ticks*dt))<0.0002);
        REQUIRE(std::abs(actual.vz-(p.vz+acceleration[2]*ticks*dt))<0.0002);
    }
    REQUIRE(simulation.ticks()==ticks);
    const auto replay_digest=digest(simulation.state());
    simulation.reset();
    REQUIRE(simulation.ticks()==0);
    REQUIRE(compare(initial,simulation.state()).equal);
    for (int i=0;i<ticks;++i) { (void)simulation.step_serial(); }
    REQUIRE(digest(simulation.state())==replay_digest);
    ParticleSimulation stationary(std::vector<Particle>{{1,2,3,0,0,0}},{0,0,0});
    (void)stationary.step_serial();
    REQUIRE(stationary.state()[0]==Particle{1,2,3,0,0,0});
}
TEST_CASE("Serial and pooled particle execution agree at every tick and partition boundary") {
    for (const std::size_t workers:{1U,2U,4U}) {
        JobSystem jobs(workers);
        for (const std::size_t count:{0U,1U,2U,3U,7U,1025U}) {
            const Config config{count,17};
            ParticleSimulation serial(config),parallel(config);
            for (int tick=0;tick<12;++tick) {
                (void)serial.step_serial();
                const auto stats=parallel.step_parallel(jobs);
                REQUIRE(stats.particles==count);
                REQUIRE(stats.worker_tasks==std::min(count,workers));
                const auto comparison=compare(serial.state(),parallel.state());
                REQUIRE(comparison.equal);
                REQUIRE(comparison.max_absolute_error==0);
                REQUIRE(serial.ticks()==parallel.ticks());
            }
            parallel.reset();
            REQUIRE(compare(initial_particles(count,17),parallel.state()).equal);
        }
    }
    JobSystem jobs(3);
    for (const std::size_t count:{1000U,10000U,100000U}) {
        ParticleSimulation serial(Config{count,0}),parallel(Config{count,0});
        for (int tick=0;tick<12;++tick) {
            (void)serial.step_serial();
            REQUIRE(parallel.step_parallel(jobs).worker_tasks==3);
            REQUIRE(compare(serial.state(),parallel.state()).equal);
        }
        REQUIRE(digest(serial.state())==digest(parallel.state()));
    }
}
TEST_CASE("Particle validation and failed jobs preserve the committed state") {
    const auto invalid=std::numeric_limits<float>::quiet_NaN();
    REQUIRE_THROWS_AS(ParticleSimulation(Config{1,1,{invalid,0,0}}),std::invalid_argument);
    const std::vector<Particle> bad{{0,0,0,invalid,0,0}};
    REQUIRE_THROWS_AS(ParticleSimulation(bad),std::invalid_argument);
    auto initial=initial_particles(17,3);
    initial.back().x=std::numeric_limits<float>::max();
    initial.back().vx=std::numeric_limits<float>::max();
    ParticleSimulation serial(initial),parallel(initial);
    JobSystem jobs(4);
    REQUIRE_THROWS_AS(serial.step_serial(),std::overflow_error);
    REQUIRE_THROWS_AS(parallel.step_parallel(jobs),std::overflow_error);
    REQUIRE(compare(initial,serial.state()).equal);
    REQUIRE(compare(initial,parallel.state()).equal);
    REQUIRE(parallel.ticks()==0);
    REQUIRE(serial.ticks()==0);
    auto healthy=initial_particles(1025,7);
    ParticleSimulation simulation(healthy);
    const auto nested=jobs.submit([&] { (void)simulation.step_parallel(jobs); });
    REQUIRE_THROWS_AS(nested.wait(),std::logic_error);
    const auto nested_reset=jobs.submit([&] { simulation.reset(); });
    REQUIRE_THROWS_AS(nested_reset.wait(),std::logic_error);
    REQUIRE(compare(healthy,simulation.state()).equal);
    jobs.shutdown();
    REQUIRE_THROWS_AS(simulation.step_parallel(jobs),std::runtime_error);
    REQUIRE(compare(healthy,simulation.state()).equal);
    REQUIRE(simulation.ticks()==0);
    REQUIRE(simulation.step_serial().particles==healthy.size());
    REQUIRE(simulation.ticks()==1);
}
TEST_CASE("Particle comparison checks all scalars and rejects nonfinite matching data") {
    const auto expected=initial_particles(3,1);
    auto actual=expected;
    actual[1].vz+=1;
    auto result=compare(expected,actual);
    REQUIRE_FALSE(result.equal);
    REQUIRE(result.mismatched_particles==1);
    REQUIRE(result.max_absolute_error==1);
    actual[2].x=std::numeric_limits<float>::infinity();
    result=compare(actual,actual);
    REQUIRE_FALSE(result.equal);
    REQUIRE(result.mismatched_particles==1);
    REQUIRE(std::isinf(result.max_absolute_error));
    REQUIRE_THROWS_AS(compare(expected,{}),std::invalid_argument);
}
TEST_CASE("Particle CLI reports actual execution and validates input") {
    for (const auto backend:{"serial","parallel","compare"}) {
        const std::vector<std::string_view> args{"--backend",backend,"--particles","7","--steps","3","--seed","0"};
        std::ostringstream out;
        run(parse_options(args),out);
        REQUIRE(out.str().find("particles=7 seed=0 ticks=3")!=std::string::npos);
        REQUIRE(out.str().find(std::string("backend=")+backend)!=std::string::npos);
        REQUIRE(out.str().find(std::string("cpu_parity=")+(std::string_view(backend)=="compare" ? "exact-every-tick" : "not-checked"))!=std::string::npos);
    }
    for (const auto& args:std::vector<std::vector<std::string_view>>{
        {"--particles","0"},{"--particles","100001"},{"--particles","-1"},{"--steps","2x"},
        {"--steps",""},{"--steps","18446744073709551616"},{"--workers","65"},{"--workers","0"},
        {"--backend","cuda"},{"--backend","serial","--workers","4"},{"--seed","4294967296"},
        {"--seed","-1"},{"--steps","1","--steps","2"},{"--steps"},{"--unknown"},{"--help","--unknown"}}) {
        REQUIRE_THROWS_AS(parse_options(args),std::invalid_argument);
    }
}

#include <coresim/particles/SoASimulation.hpp>
#include <coresim/particles/ParticleBenchmark.hpp>
TEST_CASE("SoA matches AoS at every tick, resets, and preserves state on worker failure") {
    JobSystem jobs(3);
    for (const std::size_t count : {0U, 1U, 1025U, 100000U}) {
        Config config{count, 19};
        ParticleSimulation reference(config);
        SoASimulation soa(config);
        const auto initial = soa.snapshot();
        REQUIRE(soa.owned_capacity_bytes() >= count * 3 * sizeof(Particle));
        for (int i=0; i<8; ++i) {
            (void)reference.step_serial();
            if (i%2) { (void)soa.step_parallel(jobs); } else { (void)soa.step_serial(); }
            REQUIRE(compare(reference.state(), soa.snapshot()).equal);
        }
        soa.reset();
        REQUIRE(soa.ticks()==0);
        REQUIRE(initial==soa.snapshot());
    }
    auto bad=initial_particles(31,1);
    bad.back().x=bad.back().vx=std::numeric_limits<float>::max();
    SoASimulation soa(bad);
    REQUIRE_THROWS_AS(soa.step_parallel(jobs),std::overflow_error);
    REQUIRE(soa.ticks()==0);
    REQUIRE(soa.snapshot()==bad);
}
TEST_CASE("Benchmark verifies measured steps independently of warmup and reports task count") {
    BenchmarkOptions o;
    o.count=17; o.steps=7; o.workers=3;
    std::string expected;
    for (bool soa : {false,true}) for (bool parallel : {false,true}) for (unsigned warmup : {0U,5U}) {
        o.soa=soa; o.parallel=parallel; o.warmup=warmup;
        auto r=benchmark(o);
        if (expected.empty()) { expected=r.state_digest; }
        REQUIRE(r.state_digest==expected);
        REQUIRE(r.worker_tasks==(parallel ? 21 : 0));
        REQUIRE(r.elapsed_ms>0);
        if (r.peak_rss_after) { REQUIRE(*r.peak_rss_after>=*r.peak_rss_baseline); }
    }
}
