#include <coresim/core/Profiler.hpp>
#include <coresim/physics/PhysicsSystem.hpp>
#include <coresim/scene/World.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <stdexcept>
using namespace coresim;
namespace {
Profiler::Clock::time_point current;
unsigned reads;
auto now() -> Profiler::Clock::time_point { ++reads; return current; }
const ProfileStats& stats(const Profiler& p, ProfileSection s) { return p.stats()[static_cast<std::size_t>(s)]; }
}
TEST_CASE("Profiler aggregates repeated samples and rejects invalid durations") {
    Profiler p;
    for (double ms : {0.0, 4.0, 2.0}) { p.record(ProfileSection::frame, ms); }
    const auto& s = stats(p, ProfileSection::frame);
    REQUIRE(s.count == 3); REQUIRE(s.total_ms == 6); REQUIRE(s.average_ms() == 2);
    REQUIRE(s.min_ms == 0); REQUIRE(s.max_ms == 4);
    p.record(ProfileSection::frame, -1);
    p.record(ProfileSection::frame, std::numeric_limits<double>::infinity());
    p.record(ProfileSection::frame, std::numeric_limits<double>::quiet_NaN());
    p.record(ProfileSection::count, 1);
    REQUIRE(s.count == 3);
    p.reset(); REQUIRE(s.count == 0); REQUIRE(s.average_ms() == 0);
}
TEST_CASE("RAII profiler handles nesting exceptions pause and reset without stale samples") {
    current = {}; reads = 0;
    Profiler p(now);
    {
        ScopedProfiler frame(&p, ProfileSection::frame);
        current += std::chrono::milliseconds(1);
        try {
            ScopedProfiler physics(&p, ProfileSection::physics);
            current += std::chrono::milliseconds(2);
            throw std::runtime_error("unwind");
        } catch (const std::runtime_error&) {}
        current += std::chrono::milliseconds(1);
    }
    REQUIRE(stats(p, ProfileSection::frame).total_ms == 4);
    REQUIRE(stats(p, ProfileSection::physics).total_ms == 2);
    { ScopedProfiler stale(&p, ProfileSection::frame); p.reset(); }
    REQUIRE(stats(p, ProfileSection::frame).count == 0);
    { ScopedProfiler stale(&p, ProfileSection::frame); p.set_enabled(false); p.set_enabled(true); }
    REQUIRE(stats(p, ProfileSection::frame).count == 0);
    p.set_enabled(false);
    const auto before = reads;
    { ScopedProfiler disabled(&p, ProfileSection::frame); ScopedProfiler absent(nullptr, ProfileSection::frame); }
    p.record(ProfileSection::frame, 1);
    REQUIRE(reads == before); REQUIRE(stats(p, ProfileSection::frame).count == 0);
}
TEST_CASE("Physics profile records one phase sample per fixed tick without changing results") {
    World a, b;
    for (World* world : {&a, &b}) {
        const auto e = world->create(); world->set_transform(e); world->set_rigid_body(e);
        world->set_collider(e, SphereCollider(1));
    }
    PhysicsSystem physics;
    Profiler p;
    for (int tick = 0; tick < 5; ++tick) { physics.step(a, 1.0F/120, nullptr, &p); physics.step(b, 1.0F/120); }
    REQUIRE(a.transforms()[0].value.position == b.transforms()[0].value.position);
    for (auto section : {ProfileSection::physics, ProfileSection::broad_phase, ProfileSection::narrow_phase, ProfileSection::solver}) {
        REQUIRE(stats(p, section).count == 5); REQUIRE(stats(p, section).total_ms >= 0);
    }
    p.set_enabled(false); physics.step(a, 1.0F/120, nullptr, &p);
    REQUIRE(stats(p, ProfileSection::physics).count == 5);
}
