#include <coresim/benchmark/LayoutBenchmark.hpp>
#include <coresim/jobs/JobSystem.hpp>
#include <charconv>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <locale>
#include <memory>
#include <stdexcept>
#include <string_view>

int main(int argc, char** argv) {
    try {
        constexpr auto usage = "Usage: coresim_parallel_benchmark [--workload integration|free-flight|collisions] [--workers 0..8] [--grain 1..1000000] [--bodies 1..1000000] [--steps 1..10000] [--warmup 0..10000] [--order ordered|shuffled]\n";
        std::string_view workload = "free-flight", order = "ordered";
        unsigned bodies = 100000, steps = 100, warmup = 10, workers = 0;
        unsigned grain = static_cast<unsigned>(coresim::default_integration_grain), seen = 0;
        for (int i = 1; i < argc; i += 2) {
            const std::string_view key(argv[i]);
            if (key == "--help" && argc == 2) { std::cout << usage; return 0; }
            if (i+1 >= argc) { throw std::invalid_argument(usage); }
            const std::string_view value(argv[i+1]);
            const unsigned bit = key == "--workload" ? 1U : key == "--workers" ? 2U : key == "--grain" ? 4U :
                                 key == "--bodies" ? 8U : key == "--steps" ? 16U : key == "--warmup" ? 32U :
                                 key == "--order" ? 64U : 0U;
            if (!bit || (seen & bit)) { throw std::invalid_argument(usage); }
            seen |= bit;
            if (key == "--workload") {
                workload = value;
                if (value != "integration" && value != "free-flight" && value != "collisions") { throw std::invalid_argument(usage); }
            } else if (key == "--order") {
                order = value;
                if (value != "ordered" && value != "shuffled") { throw std::invalid_argument(usage); }
            } else {
                unsigned n{};
                const auto [end,error] = std::from_chars(value.data(),value.data()+value.size(),n);
                if (error != std::errc{} || end != value.data()+value.size()) { throw std::invalid_argument(usage); }
                if (key == "--bodies") { bodies = n; }
                else if (key == "--steps") { steps = n; }
                else if (key == "--warmup") { warmup = n; }
                else if (key == "--workers") { workers = n; }
                else { grain = n; }
            }
        }
        if (!bodies || bodies > 1000000 || !steps || steps > 10000 || warmup > 10000 ||
            workers > 8 || !grain || grain > 1000000) { throw std::invalid_argument(usage); }
        coresim::World world;
        coresim::make_layout_world(world,bodies,order == "shuffled");
        if (workload == "free-flight") {
            while (!world.colliders().empty()) { world.remove_collider(world.colliders().back().entity); }
        }
        // Pool startup, scene setup and checksum reduction are outside every timed tick.
        std::unique_ptr<coresim::JobSystem> jobs;
        if (workers) { jobs = std::make_unique<coresim::JobSystem>(workers); }
        coresim::PhysicsSystem physics;
        coresim::CollisionStats stats;
        const auto step = [&] {
            if (workload == "integration") {
                coresim::integrate_bodies(world,{0,-9.81F,0},1.0F/120,coresim::BodyIteration::lookup,jobs.get(),grain);
            } else { physics.step(world,1.0F/120,&stats,nullptr,jobs.get(),grain); }
        };
        for (unsigned i = 0; i < warmup; ++i) { step(); }
        std::cout.imbue(std::locale::classic());
        std::cout << std::fixed << std::setprecision(9);
        std::cout << "schema,workload,order,build_type,compiler,bodies,workers,grain,step,warmup,total_ms,collision_ms,candidates,contacts,checksum\n";
        for (unsigned tick = 0; tick < steps; ++tick) {
            const auto start = std::chrono::steady_clock::now();
            step();
            const double elapsed = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            const auto checksum = coresim::layout_checksum(world);
            std::cout << "1," << workload << ',' << order << ',' << CORESIM_BUILD_TYPE << ',' << CORESIM_COMPILER
                      << ',' << bodies << ',' << workers << ',' << grain << ',' << tick << ',' << warmup
                      << ',' << elapsed << ',' << stats.total_collision_ms << ',' << stats.candidate_pairs
                      << ',' << stats.contacts << ',' << checksum << '\n';
        }
        std::cout.flush();
        if (!std::cout) { throw std::runtime_error("Could not write scaling CSV"); }
        return 0;
    } catch (const std::exception& error) { std::cerr << "CoreSim: " << error.what() << '\n'; return 1; }
}
