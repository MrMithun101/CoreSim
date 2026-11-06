#pragma once
#include <coresim/particles/ParticleSimulation.hpp>
#include <iosfwd>
#include <span>
#include <string_view>
namespace coresim::particles {
enum class Backend { serial, parallel, compare };
struct RunOptions {
    Config config;
    std::size_t steps = 120;
    std::size_t workers = 4;
    Backend backend = Backend::compare;
    bool help = false;
};
inline constexpr std::string_view runner_usage =
    "coresim_particle_runner [--backend serial|parallel|compare] [--particles 1..100000] "
    "[--steps 1..10000] [--workers 1..64] [--seed uint32] [--help]\n";
[[nodiscard]] RunOptions parse_options(std::span<const std::string_view> arguments);
void run(const RunOptions& options, std::ostream& output);
}
