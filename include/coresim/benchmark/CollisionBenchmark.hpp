#pragma once
#include <cstdint>
#include <iosfwd>
#include <span>
#include <string_view>

namespace coresim {
struct BenchmarkOptions {
    std::uint32_t bodies = 1000;
    std::uint32_t steps = 10;
    std::uint32_t warmup = 2;
};
inline constexpr auto benchmark_usage =
    "--benchmark collision-naive [--bodies 1..100000] [--steps 1..10000] [--warmup 0..10000]\n";
BenchmarkOptions parse_benchmark_options(std::span<const std::string_view> arguments);
void run_collision_benchmark(const BenchmarkOptions& options, std::ostream& output);
} // namespace coresim
