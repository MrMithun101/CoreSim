#pragma once
#include <coresim/physics/CollisionSystem.hpp>
#include <cstdint>
#include <iosfwd>
#include <span>
#include <string_view>

namespace coresim {
struct BenchmarkOptions {
    std::uint32_t bodies = 1000;
    std::uint32_t steps = 10;
    std::uint32_t warmup = 2;
    BroadPhase mode = BroadPhase::naive;
    float cell_size = 3.0F;
    bool paired = false;
};
inline constexpr auto benchmark_usage =
    "--benchmark collision-naive|collision-spatial [--bodies 1..100000] [--steps 1..10000] "
    "[--warmup 0..10000] [--cell-size positive-number] [--scene separated-spheres|paired-spheres]\n";
BenchmarkOptions parse_benchmark_options(std::span<const std::string_view> arguments);
void run_collision_benchmark(const BenchmarkOptions& options, std::ostream& output);
} // namespace coresim
