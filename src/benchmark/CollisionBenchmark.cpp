#include <coresim/benchmark/CollisionBenchmark.hpp>
#include <coresim/physics/PhysicsSystem.hpp>
#include <coresim/scene/World.hpp>
#include <charconv>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <locale>
#include <ostream>
#include <stdexcept>
#include <string>

namespace coresim {
namespace {
void validate(const BenchmarkOptions& options) {
    if (options.bodies == 0 || options.bodies > 100000 || options.steps == 0 ||
        options.steps > 10000 || options.warmup > 10000 ||
        !std::isfinite(options.cell_size) || options.cell_size <= 0) {
        throw std::invalid_argument(benchmark_usage);
    }
}
} // namespace
BenchmarkOptions parse_benchmark_options(std::span<const std::string_view> arguments) {
    BenchmarkOptions options;
    unsigned seen = 0;
    for (std::size_t i = 0; i < arguments.size(); i += 2) {
        if (i + 1 >= arguments.size()) { throw std::invalid_argument(benchmark_usage); }
        const auto key = arguments[i];
        const auto value = arguments[i + 1];
        const unsigned bit = key == "--benchmark" ? 1U : key == "--bodies" ? 2U :
                             key == "--steps" ? 4U : key == "--warmup" ? 8U :
                             key == "--cell-size" ? 16U : key == "--scene" ? 32U : 0U;
        if (bit == 0 || (seen & bit) != 0) { throw std::invalid_argument(benchmark_usage); }
        seen |= bit;
        if (key == "--benchmark") {
            if (value == "collision-spatial") { options.mode = BroadPhase::spatial_hash; }
            else if (value != "collision-naive") { throw std::invalid_argument(benchmark_usage); }
            continue;
        }
        if (key == "--cell-size") {
            const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), options.cell_size);
            if (error != std::errc{} || end != value.data() + value.size()) {
                throw std::invalid_argument(benchmark_usage);
            }
            continue;
        }
        if (key == "--scene") {
            if (value == "paired-spheres") { options.paired = true; }
            else if (value != "separated-spheres") { throw std::invalid_argument(benchmark_usage); }
            continue;
        }
        std::uint32_t number = 0;
        const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), number);
        if (error != std::errc{} || end != value.data() + value.size()) {
            throw std::invalid_argument(benchmark_usage);
        }
        if (key == "--bodies") { options.bodies = number; }
        else if (key == "--steps") { options.steps = number; }
        else { options.warmup = number; }
    }
    if ((seen & 1U) == 0) { throw std::invalid_argument(benchmark_usage); }
    validate(options);
    return options;
}
void run_collision_benchmark(const BenchmarkOptions& options, std::ostream& output) {
    validate(options);
    World world;
    std::uint32_t side = 1;
    const auto sites = options.paired ? (options.bodies + 1) / 2 : options.bodies;
    while (side * side * side < sites) { ++side; }
    for (std::uint32_t i = 0; i < options.bodies; ++i) {
        const auto entity = world.create();
        const auto site = options.paired ? i / 2 : i;
        const float spacing = options.paired ? 6.0F : 3.0F;
        const float offset = options.paired ? 1.5F * static_cast<float>(i % 2) : 0.0F;
        world.set_transform(entity).position = {
            spacing * static_cast<float>(site % side) + offset,
            spacing * static_cast<float>((site / side) % side),
            spacing * static_cast<float>(site / (side * side))};
        world.set_collider(entity, SphereCollider(1));
        world.set_rigid_body(entity, RigidBody(1, 0));
    }
    PhysicsSystem physics(glm::vec3(0), options.mode, options.cell_size);
    CollisionStats stats;
    constexpr float timestep = 1.0F / 120.0F;
    for (std::uint32_t i = 0; i < options.warmup; ++i) { physics.step(world, timestep, &stats); }
    // A fixed locale/precision keeps redirected output machine-readable on all hosts.
    const auto previous_locale = output.getloc();
    const auto previous_flags = output.flags();
    const auto previous_precision = output.precision();
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(6);
    output << "schema,benchmark,scene,build_type,compiler,bodies,step,warmup,timestep_s,candidate_pairs,"
              "collision_checks,contacts,correction_checks,broad_phase_ms,narrow_phase_ms,solver_ms,"
              "physics_step_ms,total_frame_ms,cell_size,hash_build_ms,query_ms,total_collision_ms,pair_reduction_percent\n";
    using Clock = std::chrono::steady_clock;
    for (std::uint32_t i = 0; i < options.steps; ++i) {
        const auto frame_start = Clock::now();
        const auto physics_start = Clock::now();
        physics.step(world, timestep, &stats);
        const auto physics_end = Clock::now();
        const auto frame_end = Clock::now();
        const auto ms = [](auto start, auto end) {
            return std::chrono::duration<double, std::milli>(end - start).count();
        };
        output << "2," << (options.mode == BroadPhase::naive ? "collision-naive" : "collision-spatial")
               << ',' << (options.paired ? "paired-spheres" : "separated-spheres") << ',' << CORESIM_BUILD_TYPE << ','
               << CORESIM_COMPILER << ',' << options.bodies << ',' << i << ',' << options.warmup
               << ',' << timestep << ',' << stats.candidate_pairs << ',' << stats.collision_checks
               << ',' << stats.contacts << ',' << stats.correction_checks << ',' << stats.broad_phase_ms
               << ',' << stats.narrow_phase_ms << ',' << stats.solver_ms << ','
               << ms(physics_start, physics_end) << ',' << ms(frame_start, frame_end) << ','
               << options.cell_size << ',' << stats.hash_build_ms << ',' << stats.query_ms << ','
               << stats.total_collision_ms << ',' << stats.pair_reduction_percent << '\n';
    }
    output.flush();
    const bool failed = !output;
    output.imbue(previous_locale);
    output.flags(previous_flags);
    output.precision(previous_precision);
    if (failed) { throw std::runtime_error("Could not write benchmark CSV"); }
}
} // namespace coresim
