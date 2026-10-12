#include <coresim/jobs/JobSystem.hpp>
#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <future>
#include <latch>
#include <stdexcept>
#include <vector>
using namespace coresim;
TEST_CASE("Thousands of jobs execute exactly once and publish ordinary writes") {
    for (std::size_t workers : {1U, 2U, 4U}) {
        JobSystem system(workers);
        REQUIRE(system.worker_count() == workers);
        std::vector<int> values(12000);
        JobGroup group;
        for (std::size_t i = 0; i < values.size(); ++i) { system.submit(group, [&,i] { values[i] += 1; }); }
        group.wait();
        for (auto value : values) { REQUIRE(value == 1); }
        group.wait();
        REQUIRE_THROWS_AS(system.submit(group, [] {}), std::logic_error);
    }
}
TEST_CASE("Concurrent producers and waiters share a sealed completion group") {
    JobSystem system(4);
    JobGroup group;
    std::atomic<int> total{};
    std::vector<std::thread> producers;
    for (int p = 0; p < 6; ++p) {
        producers.emplace_back([&] { for (int i = 0; i < 2000; ++i) { system.submit(group, [&] { ++total; }); } });
    }
    for (auto& producer : producers) { producer.join(); }
    const auto copy = group;
    auto first = std::async(std::launch::async, [&] { group.wait(); });
    auto second = std::async(std::launch::async, [&] { copy.wait(); });
    first.get(); second.get();
    REQUIRE(total == 12000);
}
TEST_CASE("Failures propagate after all siblings finish and workers remain reusable") {
    JobSystem system(2);
    JobGroup group;
    std::atomic<int> total{};
    system.submit(group, [] { throw std::runtime_error("job failed"); });
    for (int i = 0; i < 1000; ++i) { system.submit(group, [&] { ++total; }); }
    REQUIRE_THROWS_AS(group.wait(), std::runtime_error);
    REQUIRE(total == 1000);
    REQUIRE_THROWS_AS(group.wait(), std::runtime_error);
    system.submit([&] { ++total; }).wait();
    REQUIRE(total == 1001);
    REQUIRE_THROWS_AS(system.submit(Job{}), std::invalid_argument);
    JobGroup empty; empty.wait();
    REQUIRE_THROWS_AS(system.submit(empty, [] {}), std::logic_error);
}
TEST_CASE("Shutdown drains accepted work rejects later work and is idempotent") {
    JobSystem system(2);
    std::atomic<int> completed{}, accepted{}, rejected{};
    JobGroup group;
    std::latch start(1);
    auto producer = std::async(std::launch::async, [&] {
        start.wait();
        for (int i = 0; i < 10000; ++i) {
            try { system.submit(group, [&] { ++completed; }); ++accepted; }
            catch (const std::runtime_error&) { ++rejected; }
        }
    });
    auto stop = std::async(std::launch::async, [&] { start.wait(); system.shutdown(); });
    auto second_stop = std::async(std::launch::async, [&] { start.wait(); system.shutdown(); });
    start.count_down();
    producer.get(); stop.get(); second_stop.get(); group.wait();
    REQUIRE(accepted + rejected == 10000);
    REQUIRE(completed == accepted);
    system.shutdown();
    REQUIRE_THROWS_AS(system.submit([] {}), std::runtime_error);
}
TEST_CASE("Destructor drains queued jobs and group handles survive pool lifetime") {
    JobGroup group;
    std::atomic<int> total{};
    {
        JobSystem system(1);
        for (int i = 0; i < 5000; ++i) { system.submit(group, [&] { ++total; }); }
    }
    group.wait(); REQUIRE(total == 5000);
    for (int cycle = 0; cycle < 30; ++cycle) {
        JobSystem system(2);
        for (int burst = 0; burst < 10; ++burst) { system.submit([&] { ++total; }).wait(); }
    }
    REQUIRE(total == 5300);
}
TEST_CASE("Worker waits and shutdown fail promptly instead of starving the pool") {
    JobSystem system(1);
    auto wait = system.submit([] { JobGroup empty; empty.wait(); });
    REQUIRE_THROWS_AS(wait.wait(), std::logic_error);
    auto stop = system.submit([&] { system.shutdown(); });
    REQUIRE_THROWS_AS(stop.wait(), std::logic_error);
    std::atomic<int> total{};
    JobGroup children;
    system.submit([&] { system.submit(children, [&] { ++total; }); }).wait();
    children.wait(); REQUIRE(total == 1);
}
TEST_CASE("Completion waits for callable capture release") {
    JobSystem system(1);
    auto captured = std::make_shared<int>(7);
    std::weak_ptr<int> weak = captured;
    JobGroup group;
    system.submit(group, [keep = std::move(captured)] { (void)keep; });
    group.wait(); REQUIRE(weak.expired());
}

TEST_CASE("Wait and submission linearize at the group seal") {
    JobSystem system(2);
    for (int round = 0; round < 100; ++round) {
        JobGroup group;
        std::atomic<int> completed{};
        std::latch start(1);
        auto submitter = std::async(std::launch::async, [&] {
            start.wait();
            try { system.submit(group, [&] { ++completed; }); return 1; }
            catch (const std::logic_error&) { return 0; }
        });
        auto waiter = std::async(std::launch::async, [&] { start.wait(); group.wait(); });
        start.count_down();
        const int accepted = submitter.get(); waiter.get();
        REQUIRE(completed == accepted);
    }
}
TEST_CASE("Dropped group handles do not abandon work and worker lifecycle calls are rejected") {
    JobSystem system(2);
    std::atomic<int> total{};
    for (int i = 0; i < 1000; ++i) { (void)system.submit([&] { ++total; }); }
    auto nested = system.submit([] { JobSystem disallowed(1); });
    REQUIRE_THROWS_AS(nested.wait(), std::logic_error);
    system.shutdown(); REQUIRE(total == 1000);
    JobGroup original;
    const JobGroup moved = std::move(original);
    REQUIRE_THROWS_AS(original.wait(), std::invalid_argument);
    JobSystem other(1);
    REQUIRE_THROWS_AS(other.submit(original, [] {}), std::invalid_argument);
    moved.wait();
}
