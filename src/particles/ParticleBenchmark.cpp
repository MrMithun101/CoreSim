#include <coresim/particles/ParticleBenchmark.hpp>
#include <coresim/particles/SoASimulation.hpp>
#include <coresim/jobs/JobSystem.hpp>
#include <charconv>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>
#if defined(__APPLE__) || defined(__linux__)
#include <sys/resource.h>
#endif
namespace coresim::particles {
namespace {
using Clock=std::chrono::steady_clock;
std::uint64_t number(std::string_view text,std::uint64_t maximum,bool zero=false) {
    std::uint64_t value{};
    const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),value);
    if (error!=std::errc{} || end!=text.data()+text.size() || value>maximum || (!zero && !value)) {
        throw std::invalid_argument("Invalid particle benchmark number: "+std::string(text));
    }
    return value;
}
std::optional<std::uint64_t> peak_rss() {
#if defined(__APPLE__) || defined(__linux__)
    rusage usage{};
    if (getrusage(RUSAGE_SELF,&usage)!=0 || usage.ru_maxrss<0) { throw std::runtime_error("getrusage failed"); }
    auto bytes=static_cast<std::uint64_t>(usage.ru_maxrss);
#if defined(__linux__)
    if (bytes>std::numeric_limits<std::uint64_t>::max()/1024) { throw std::overflow_error("RSS byte count overflow"); }
    bytes*=1024;
#endif
    return bytes;
#else
    return std::nullopt;
#endif
}
std::vector<Particle> snapshot(const ParticleSimulation& simulation) {
    return {simulation.state().begin(),simulation.state().end()};
}
std::vector<Particle> snapshot(const SoASimulation& simulation) { return simulation.snapshot(); }
template<class Simulation> BenchmarkResult measure(const BenchmarkOptions& options) {
    BenchmarkResult result;
    result.options=options;
    result.peak_rss_baseline=peak_rss();
    const Config config{options.count,options.seed};
    Simulation simulation(config);
    std::unique_ptr<JobSystem> jobs;
    if (options.parallel) { jobs=std::make_unique<JobSystem>(options.workers); }
    const auto step=[&] { return jobs ? simulation.step_parallel(*jobs) : simulation.step_serial(); };
    for (std::size_t i=0; i<options.warmup; ++i) { (void)step(); }
    simulation.reset();
    result.owned_capacity_bytes=simulation.owned_capacity_bytes();
    result.peak_rss_ready=peak_rss();
    const auto start=Clock::now();
    for (std::size_t i=0; i<options.steps; ++i) { result.worker_tasks+=step().worker_tasks; }
    result.elapsed_ms=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
    result.peak_rss_after=peak_rss(); // Before snapshots/reference allocations contaminate the peak.
    if (!(result.elapsed_ms>0) || !std::isfinite(result.elapsed_ms)) { throw std::runtime_error("Invalid benchmark duration"); }
    const auto actual=snapshot(simulation);
    ParticleSimulation reference(config);
    for (std::size_t i=0; i<options.steps; ++i) { (void)reference.step_serial(); }
    if (simulation.ticks()!=options.steps || !compare(reference.state(),actual).equal) {
        throw std::runtime_error("Particle benchmark final-state verification failed");
    }
    std::ostringstream digest_text;
    digest_text << std::hex << digest(actual);
    result.state_digest=digest_text.str();
    return result;
}
std::string json_quote(const std::string& value) {
    std::ostringstream out;
    out << '"';
    for (const char ch:value) {
        const auto c=static_cast<unsigned char>(ch);
        if (c=='"' || c=='\\') { out << '\\' << static_cast<char>(c); }
        else if (c<32) { out << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(c); }
        else { out << static_cast<char>(c); }
    }
    out << '"';
    return out.str();
}
std::string csv_quote(const std::string& value) {
    std::string out="\"";
    for (const char c:value) { if (c=='"') { out+='"'; } out+=c; }
    return out+'"';
}
template<class T> std::string numeric(T value) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(17) << value;
    return out.str();
}
std::string optional_number(std::optional<std::uint64_t> value) { return value ? numeric(*value) : ""; }
struct Field { std::string key,value; bool string=false; };
}
BenchmarkOptions parse_benchmark_options(std::span<const std::string_view> arguments) {
    BenchmarkOptions options;
    std::set<std::string_view> seen;
    for (std::size_t i=0; i<arguments.size(); ++i) {
        const auto key=arguments[i];
        if (!seen.insert(key).second) { throw std::invalid_argument("Duplicate benchmark option"); }
        if (key=="--help") { options.help=true; continue; }
        if (key!="--layout" && key!="--backend" && key!="--particles" && key!="--steps" && key!="--warmup" && key!="--workers" && key!="--seed" && key!="--format") {
            throw std::invalid_argument("Unknown benchmark option: "+std::string(key));
        }
        if (++i==arguments.size()) { throw std::invalid_argument("Missing benchmark value"); }
        const auto value=arguments[i];
        if (key=="--layout") {
            if (value!="aos" && value!="soa") { throw std::invalid_argument("Layout must be aos or soa"); }
            options.soa=value=="soa";
        } else if (key=="--backend") {
            if (value!="serial" && value!="parallel") { throw std::invalid_argument("Backend must be serial or parallel"); }
            options.parallel=value=="parallel";
        } else if (key=="--format") {
            if (value!="json" && value!="csv") { throw std::invalid_argument("Format must be json or csv"); }
            options.csv=value=="csv";
        } else if (key=="--particles") { options.count=static_cast<std::size_t>(number(value,maximum_particles)); }
        else if (key=="--steps") { options.steps=static_cast<std::size_t>(number(value,100000)); }
        else if (key=="--warmup") { options.warmup=static_cast<std::size_t>(number(value,10000,true)); }
        else if (key=="--workers") { options.workers=static_cast<std::size_t>(number(value,64)); }
        else { options.seed=static_cast<std::uint32_t>(number(value,std::numeric_limits<std::uint32_t>::max(),true)); }
    }
    if (!options.parallel && seen.contains("--workers")) { throw std::invalid_argument("--workers requires parallel backend"); }
    return options;
}
BenchmarkResult benchmark(const BenchmarkOptions& options) {
    if (options.count==0 || options.count>maximum_particles || options.steps==0 || options.steps>100000 || options.warmup>10000 || options.workers==0 || options.workers>64) {
        throw std::invalid_argument("Particle benchmark options out of range");
    }
    return options.soa ? measure<SoASimulation>(options) : measure<ParticleSimulation>(options);
}
void write_benchmark(const BenchmarkResult& r,std::ostream& output) {
    const auto& o=r.options;
    const std::vector<Field> fields{
        {"schema","1"},{"workload","constant-acceleration-v1",true},
        {"layout",o.soa ? "soa" : "aos",true},{"backend",o.parallel ? "parallel" : "serial",true},
        {"particles",numeric(o.count)},{"steps",numeric(o.steps)},{"warmup",numeric(o.warmup)},{"seed",numeric(o.seed)},
        {"timestep",numeric(timestep)},{"acceleration_x","0"},{"acceleration_y",numeric(default_acceleration[1])},{"acceleration_z","0"},
        {"workers",numeric(o.parallel ? o.workers : 0)},{"worker_tasks",numeric(r.worker_tasks)},
        {"elapsed_ms",numeric(r.elapsed_ms)},{"ms_per_tick",numeric(r.elapsed_ms/static_cast<double>(o.steps))},
        {"updates_per_second",numeric(static_cast<double>(o.count)*static_cast<double>(o.steps)*1000.0/r.elapsed_ms)},
        {"owned_capacity_bytes",numeric(r.owned_capacity_bytes)},
        {"peak_process_rss_baseline_bytes",optional_number(r.peak_rss_baseline)},
        {"peak_process_rss_ready_bytes",optional_number(r.peak_rss_ready)},
        {"peak_process_rss_after_bytes",optional_number(r.peak_rss_after)},
        {"verification","exact-final-state",true},{"digest",r.state_digest,true},
        {"build_type",CORESIM_BUILD_TYPE,true},{"compiler",CORESIM_COMPILER,true},
        {"sanitizer",CORESIM_SANITIZER_NAME,true},{"build_os",CORESIM_BUILD_OS,true},{"build_arch",CORESIM_BUILD_ARCH,true},
        {"logical_cpus",numeric(std::thread::hardware_concurrency())},{"clock","steady_clock",true},
        {"clock_period_ns",numeric(1e9*static_cast<double>(Clock::period::num)/static_cast<double>(Clock::period::den))}
    };
    if (o.csv) {
        for (std::size_t i=0; i<fields.size(); ++i) { output << (i ? "," : "") << fields[i].key; }
        output << '\n';
        for (std::size_t i=0; i<fields.size(); ++i) { output << (i ? "," : "") << csv_quote(fields[i].value); }
        output << '\n';
    } else {
        output << '{';
        for (std::size_t i=0; i<fields.size(); ++i) {
            const auto& f=fields[i];
            output << (i ? "," : "") << json_quote(f.key) << ':'
                   << (f.string ? json_quote(f.value) : f.value.empty() ? "null" : f.value);
        }
        output << "}\n";
    }
}
}
