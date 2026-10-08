#include <coresim/core/FrameTimer.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <stdexcept>

using namespace std::chrono_literals;
using coresim::FrameTimer;
TEST_CASE("New timers have finite zero statistics") {
    const FrameTimer timer(FrameTimer::TimePoint{});
    REQUIRE(timer.stats().frame_count == 0);
    REQUIRE(timer.stats().fps == 0.0);
    REQUIRE(timer.stats().delta_seconds == 0.0);
    REQUIRE(timer.stats().total_frame_seconds == 0.0);
}
TEST_CASE("Delta measures start-to-start and frame time includes presentation") {
    const FrameTimer::TimePoint start{};
    FrameTimer timer(start);
    timer.begin_frame(start);
    timer.end_frame(start + 10ms);
    timer.begin_frame(start + 16ms);
    REQUIRE(timer.stats().delta_seconds == Catch::Approx(0.016));
    timer.end_frame(start + 30ms);
    REQUIRE(timer.stats().total_frame_seconds == Catch::Approx(0.014));
    REQUIRE(timer.stats().elapsed_seconds == Catch::Approx(0.030));
    REQUIRE(timer.stats().frame_count == 2);
    REQUIRE(timer.stats().fps == Catch::Approx(2.0 / 0.030));
}
TEST_CASE("Zero-duration frames avoid division by zero") {
    const FrameTimer::TimePoint start{};
    FrameTimer timer(start);
    timer.begin_frame(start);
    timer.end_frame(start);
    REQUIRE(timer.stats().fps == 0.0);
    REQUIRE(timer.stats().frame_count == 1);
}
TEST_CASE("Invalid transitions are rejected without corrupting state") {
    const FrameTimer::TimePoint start{};
    FrameTimer timer(start);
    REQUIRE_THROWS_AS(timer.end_frame(start), std::logic_error);
    REQUIRE_THROWS_AS(timer.begin_frame(start - 1ms), std::logic_error);
    timer.begin_frame(start);
    REQUIRE_THROWS_AS(timer.begin_frame(start), std::logic_error);
    REQUIRE_THROWS_AS(timer.end_frame(start - 1ms), std::logic_error);
    timer.end_frame(start + 10ms);
    REQUIRE_THROWS_AS(timer.begin_frame(start + 5ms), std::logic_error);
    timer.begin_frame(start + 20ms);
    timer.end_frame(start + 30ms);
    REQUIRE(timer.stats().frame_count == 2);
}
