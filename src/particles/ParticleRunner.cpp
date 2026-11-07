#include <coresim/particles/ParticleRunner.hpp>
#include <coresim/jobs/JobSystem.hpp>
#include <coresim/particles/CudaParticles.hpp>
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
        if (key == "--cuda-info") { options.cuda_info = true; continue; }
        if (key != "--device" && key != "--backend" && key != "--particles" && key != "--steps" && key != "--workers" && key != "--seed") {
            throw std::invalid_argument("Unknown particle option: " + std::string(key));
        }
        if (++i == arguments.size()) { throw std::invalid_argument("Missing particle option value"); }
        const auto value = arguments[i];
        if (key == "--backend") {
            if (value == "serial") { options.backend = Backend::serial; }
            else if (value == "parallel") { options.backend = Backend::parallel; }
            else if (value == "compare") { options.backend = Backend::compare; }
            else if (value == "cuda") { options.backend = Backend::cuda; }
            else { throw std::invalid_argument("Particle backend must be serial, parallel, compare or cuda"); }
        } else if (key == "--particles") { options.config.count = static_cast<std::size_t>(integer(value, maximum_particles)); }
        else if (key == "--steps") { options.steps = static_cast<std::size_t>(integer(value, 10000)); }
        else if (key == "--workers") { options.workers = static_cast<std::size_t>(integer(value, 64)); }
        else if (key == "--device") { options.device = static_cast<int>(integer(value, std::numeric_limits<int>::max(), true)); }
        else { options.config.seed = static_cast<std::uint32_t>(integer(value, std::numeric_limits<std::uint32_t>::max(), true)); }
    }
    if (options.cuda_info && seen.size() != 1) { throw std::invalid_argument("--cuda-info is a standalone inspection command"); }
    if (seen.contains("--device") && options.backend != Backend::cuda) { throw std::invalid_argument("--device requires CUDA"); }
    if (options.backend != Backend::parallel && options.backend != Backend::compare && seen.contains("--workers")) {
        throw std::invalid_argument("--workers requires parallel or compare backend");
    }
    return options;
}
void run(const RunOptions& options, std::ostream& output) {
    if (options.help) { output << runner_usage; return; }
    if (options.cuda_info) {
        const auto status = cuda_status();
        output << "cuda_compiled=" << (status.compiled ? "true" : "false") << " devices=" << status.devices.size() << '\n';
        for (const auto& d : status.devices) { output << "device=" << d.index << " name=" << d.name << " capability=" << d.major << '.' << d.minor << '\n'; }
        if (!status.diagnostic.empty()) { output << "diagnostic=" << status.diagnostic << '\n'; }
        return;
    }
    if (options.config.count == 0 || options.config.count > maximum_particles || options.steps == 0 || options.steps > 10000 || options.workers == 0 || options.workers > 64) {
        throw std::invalid_argument("Particle runner options out of range");
    }
    if (options.backend == Backend::cuda) {
        ParticleSimulation reference(options.config);
        const auto actual = integrate_cuda(reference.state(), options.config.acceleration, options.steps, options.device);
        for (std::size_t step = 0; step < options.steps; ++step) { (void)reference.step_serial(); }
        const auto comparison = compare_cuda(reference.state(), actual);
        if (!comparison.equal) {
            throw std::runtime_error("CUDA final-state mismatch: particles=" + std::to_string(comparison.mismatched_particles)
                                     + " max_absolute_error=" + std::to_string(comparison.max_absolute_error));
        }
        output << "workload=constant-acceleration-v1 backend=cuda device=" << options.device
               << " particles=" << actual.size() << " seed=" << options.config.seed << " ticks=" << options.steps
               << " cpu_parity=tolerant-final-state max_absolute_error=" << comparison.max_absolute_error
               << " abs_tolerance=" << cuda_abs_tolerance << " rel_tolerance=" << cuda_rel_tolerance
               << " digest=" << std::hex << digest(actual) << std::dec << '\n';
        return;
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
