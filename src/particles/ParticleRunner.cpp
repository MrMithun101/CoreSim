#include <coresim/particles/ParticleRunner.hpp>
#include <coresim/jobs/JobSystem.hpp>
#include <charconv>
#include <iomanip>
#include <limits>
#include <memory>
#include <ostream>
#include <set>
#include <stdexcept>
#include <string>
namespace coresim::particles {
namespace {
std::uint64_t integer(std::string_view value, std::uint64_t maximum, bool allow_zero = false) {
    std::uint64_t number{};
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), number);
    if (error != std::errc{} || end != value.data() + value.size() || number > maximum || (!allow_zero && number == 0)) {
        throw std::invalid_argument("Invalid particle option value: " + std::string(value));
    }
    return number;
}
}
RunOptions parse_options(std::span<const std::string_view> arguments) {
    RunOptions options;
    std::set<std::string_view> seen;
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        const auto key = arguments[i];
        if (!seen.insert(key).second) { throw std::invalid_argument("Duplicate particle option"); }
        if (key == "--help") { options.help = true; continue; }
        if (key != "--backend" && key != "--particles" && key != "--steps" && key != "--workers" && key != "--seed") {
            throw std::invalid_argument("Unknown particle option: " + std::string(key));
        }
        if (++i == arguments.size()) { throw std::invalid_argument("Missing particle option value"); }
        const auto value = arguments[i];
        if (key == "--backend") {
            if (value == "serial") { options.backend = Backend::serial; }
            else if (value == "parallel") { options.backend = Backend::parallel; }
            else if (value == "compare") { options.backend = Backend::compare; }
            else { throw std::invalid_argument("Particle backend must be serial, parallel or compare"); }
        } else if (key == "--particles") { options.config.count = static_cast<std::size_t>(integer(value, maximum_particles)); }
        else if (key == "--steps") { options.steps = static_cast<std::size_t>(integer(value, 10000)); }
        else if (key == "--workers") { options.workers = static_cast<std::size_t>(integer(value, 64)); }
        else { options.config.seed = static_cast<std::uint32_t>(integer(value, std::numeric_limits<std::uint32_t>::max(), true)); }
    }
    if (options.backend == Backend::serial && seen.contains("--workers")) {
        throw std::invalid_argument("--workers requires parallel or compare backend");
    }
    return options;
}
void run(const RunOptions& options, std::ostream& output) {
    if (options.help) { output << runner_usage; return; }
    if (options.config.count == 0 || options.config.count > maximum_particles || options.steps == 0 || options.steps > 10000 || options.workers == 0 || options.workers > 64) {
        throw std::invalid_argument("Particle runner options out of range");
    }
    if (options.backend != Backend::serial && options.backend != Backend::parallel && options.backend != Backend::compare) {
        throw std::invalid_argument("Invalid particle backend");
    }
    ParticleSimulation simulation(options.config);
    std::unique_ptr<JobSystem> jobs;
    if (options.backend != Backend::serial) { jobs = std::make_unique<JobSystem>(options.workers); }
    std::unique_ptr<ParticleSimulation> reference;
    if (options.backend == Backend::compare) { reference = std::make_unique<ParticleSimulation>(options.config); }
    std::size_t tasks = 0;
    for (std::size_t step = 0; step < options.steps; ++step) {
        const auto stats = jobs ? simulation.step_parallel(*jobs) : simulation.step_serial();
        tasks += stats.worker_tasks;
        if (reference) {
            (void)reference->step_serial();
            const auto comparison = compare(reference->state(), simulation.state());
            if (!comparison.equal) {
                throw std::runtime_error("Particle CPU parity failed at tick " + std::to_string(step + 1));
            }
        }
    }
    const char* name = options.backend == Backend::serial ? "serial" : options.backend == Backend::parallel ? "parallel" : "compare";
    output << "workload=constant-acceleration-v1 backend=" << name
           << " particles=" << simulation.state().size() << " seed=" << options.config.seed
           << " ticks=" << simulation.ticks() << " timestep=" << std::setprecision(9) << timestep
           << " acceleration=" << options.config.acceleration[0] << ',' << options.config.acceleration[1] << ',' << options.config.acceleration[2]
           << " workers=" << (jobs ? jobs->worker_count() : 0) << " worker_tasks=" << tasks
           << " cpu_parity=" << (reference ? "exact-every-tick" : "not-checked")
           << " digest=" << std::hex << digest(simulation.state()) << std::dec << '\n';
}
}
