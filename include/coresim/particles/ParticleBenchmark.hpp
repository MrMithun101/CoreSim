#pragma once
#include <coresim/particles/ParticleSimulation.hpp>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
namespace coresim::particles {
struct BenchmarkOptions {
    std::size_t count=1000, steps=1000, warmup=60, workers=4;
    std::uint32_t seed=1;
    bool soa=false, parallel=false, csv=false, help=false;
};
inline constexpr std::string_view benchmark_usage =
    "coresim_particle_benchmark [--layout aos|soa] [--backend serial|parallel] "
    "[--particles 1..100000] [--steps 1..100000] [--warmup 0..10000] "
    "[--workers 1..64] [--seed uint32] [--format json|csv] [--help]\n";
struct BenchmarkResult {
    BenchmarkOptions options;
    double elapsed_ms{};
    std::size_t worker_tasks{}, owned_capacity_bytes{};
    std::optional<std::uint64_t> peak_rss_baseline, peak_rss_ready, peak_rss_after;
    std::string state_digest;
};
[[nodiscard]] BenchmarkOptions parse_benchmark_options(std::span<const std::string_view> arguments);
// Times only step batches; verifies final state against serial AoS outside timing.
[[nodiscard]] BenchmarkResult benchmark(const BenchmarkOptions& options);
void write_benchmark(const BenchmarkResult& result, std::ostream& output);
}
