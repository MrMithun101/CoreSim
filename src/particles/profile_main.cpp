#include <coresim/particles/CudaParticles.hpp>
#include <coresim/particles/ParticleRunner.hpp>
#include <coresim/jobs/JobSystem.hpp>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <sstream>

using namespace coresim::particles;
namespace {
std::string quoted_json(std::string_view value) {
    std::ostringstream out;
    out << '"';
    for (char ch : value) {
        if (ch == '"' || ch == '\\') { out << '\\' << ch; }
        else if (static_cast<unsigned char>(ch) < 32) {
            out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<unsigned>(static_cast<unsigned char>(ch));
        } else { out << ch; }
    }
    return out.str()+'"';
}
}
int main(int argc, char** argv) {
    try {
        std::vector<std::string_view> args;
        std::optional<unsigned> warm;
        for (int i=1; i<argc; ++i) {
            if (std::string_view(argv[i])!="--warmup") { args.emplace_back(argv[i]); continue; }
            if (warm || ++i==argc) { throw std::invalid_argument("Duplicate/missing warmup"); }
            std::string_view v(argv[i]); unsigned n{};
            auto [end,ec]=std::from_chars(v.data(),v.data()+v.size(),n);
            if (ec!=std::errc{} || end!=v.data()+v.size() || n==0 || n>10000) { throw std::invalid_argument("warmup must be 1..10000"); }
            warm=n;
        }
        const auto o=parse_options(args);
        if (o.help) {
            std::cout<<"coresim_particle_profile --backend serial|parallel|cuda [--particles N] [--steps N] [--warmup N] [--workers N] [--device N] [--seed N]\n";
            return 0;
        }
        if (o.backend==Backend::compare || o.cuda_info) { throw std::invalid_argument("Select --backend serial, parallel or cuda"); }
        auto input=initial_particles(o.config.count,o.config.seed);
        auto jobs=o.backend==Backend::parallel ? std::make_unique<coresim::JobSystem>(o.workers) : nullptr;
        const bool gpu=o.backend==Backend::cuda;
        CudaStatus status{};
        if (gpu) {
            status=cuda_status();
            if (!status.diagnostic.empty()) { throw std::runtime_error(status.diagnostic); }
            if (static_cast<std::size_t>(o.device)>=status.devices.size()) { throw std::invalid_argument("CUDA device unavailable"); }
        }
        auto batch=[&](std::size_t ticks,CudaPhases* phases) {
            if (gpu) { return integrate_cuda(input,o.config.acceleration,ticks,o.device,phases); }
            ParticleSimulation sim(input,o.config.acceleration);
            for (std::size_t i=0;i<ticks;++i) { if (jobs) { (void)sim.step_parallel(*jobs); } else { (void)sim.step_serial(); } }
            return std::vector<Particle>(sim.state().begin(),sim.state().end());
        };
        const auto warmup=warm.value_or(60);
        (void)batch(warmup,nullptr);
        CudaPhases phases;
        const auto start=std::chrono::steady_clock::now();
        auto actual=batch(o.steps,gpu ? &phases : nullptr);
        const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        ParticleSimulation ref(input,o.config.acceleration);
        for (std::size_t i=0;i<o.steps;++i) { (void)ref.step_serial(); }
        auto cmp=gpu ? compare_cuda(ref.state(),actual) : compare(ref.state(),actual);
        if (!cmp.equal || !(ms>0)) { throw std::runtime_error("Invalid duration or final-state mismatch; max absolute error="+std::to_string(cmp.max_absolute_error)); }
        if (gpu) {
            for (auto t:{phases.upload_ms,phases.kernel_ms,phases.download_ms}) {
                if (!std::isfinite(t) || t<0) { throw std::runtime_error("Invalid device event duration"); }
            }
        }
        std::cout<<std::setprecision(17)<<"{\"schema\":1,\"workload\":\"constant-acceleration-v1\",\"backend\":"
            <<quoted_json(gpu?"cuda":jobs?"parallel":"serial")<<",\"layout\":\"aos\",\"particles\":"<<input.size()
            <<",\"seed\":"<<o.config.seed<<",\"steps\":"<<o.steps<<",\"warmup\":"<<warmup<<",\"workers\":"<<(jobs?jobs->worker_count():0)
            <<",\"build_type\":"<<quoted_json(CORESIM_BUILD_TYPE)<<",\"compiler\":"<<quoted_json(CORESIM_COMPILER)
            <<",\"sanitizer\":"<<quoted_json(CORESIM_SANITIZER_NAME)<<",\"end_to_end_ms\":"<<ms
            <<",\"max_absolute_error\":"<<cmp.max_absolute_error<<",\"verified\":true,\"reference_digest\":\""<<std::hex<<digest(ref.state())<<"\",\"actual_digest\":\""<<digest(actual)<<std::dec<<'"';
        if (gpu) {
            auto& d=status.devices[static_cast<std::size_t>(o.device)];
            std::cout<<",\"upload_ms\":"<<phases.upload_ms<<",\"kernel_ms\":"<<phases.kernel_ms<<",\"download_ms\":"<<phases.download_ms
                <<",\"device\":"<<d.index<<",\"gpu\":"<<quoted_json(d.name)<<",\"capability\":\""<<d.major<<'.'<<d.minor
                <<"\",\"driver_api\":"<<status.driver_api<<",\"runtime\":"<<status.runtime<<",\"toolkit\":"<<status.toolkit
                <<",\"abs_tolerance\":"<<cuda_abs_tolerance<<",\"rel_tolerance\":"<<cuda_rel_tolerance;
        } else { std::cout<<",\"upload_ms\":null,\"kernel_ms\":null,\"download_ms\":null"; }
        std::cout<<"}\n";
    } catch (const std::exception& e) { std::cerr<<"Particle profile: "<<e.what()<<'\n'; return 1; }
}
