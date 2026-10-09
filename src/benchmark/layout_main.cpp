#include <coresim/benchmark/LayoutBenchmark.hpp>
#include <charconv>
#include <iomanip>
#include <iostream>
#include <locale>
#include <stdexcept>
#include <string_view>

int main(int argc, char** argv) {
    try {
        constexpr auto usage = "Usage: coresim_layout_benchmark --layout lookup|dense|aos|soa [--bodies 1..1000000] [--steps 1..10000] [--warmup 0..10000] [--order ordered|shuffled]\n";
        coresim::IntegrationLayout layout = coresim::IntegrationLayout::lookup;
        std::string_view name = "lookup", order = "ordered";
        unsigned bodies = 10000, steps = 100, warmup = 10, seen = 0;
        for (int i = 1; i < argc; i += 2) {
            const std::string_view key(argv[i]);
            if (key == "--help" && argc == 2) { std::cout << usage; return 0; }
            if (i + 1 >= argc) { throw std::invalid_argument(usage); }
            const std::string_view value(argv[i+1]);
            const unsigned bit = key == "--layout" ? 1U : key == "--bodies" ? 2U : key == "--steps" ? 4U :
                                 key == "--warmup" ? 8U : key == "--order" ? 16U : 0U;
            if (!bit || (seen & bit)) { throw std::invalid_argument(usage); }
            seen |= bit;
            if (key == "--layout") {
                name = value;
                if (value == "lookup") { layout = coresim::IntegrationLayout::lookup; }
                else if (value == "dense") { layout = coresim::IntegrationLayout::dense; }
                else if (value == "aos") { layout = coresim::IntegrationLayout::aos; }
                else if (value == "soa") { layout = coresim::IntegrationLayout::soa; }
                else { throw std::invalid_argument(usage); }
            } else if (key == "--order") {
                order = value;
                if (value != "ordered" && value != "shuffled") { throw std::invalid_argument(usage); }
            } else {
                unsigned number{};
                const auto [end,error] = std::from_chars(value.data(), value.data()+value.size(), number);
                if (error != std::errc{} || end != value.data()+value.size()) { throw std::invalid_argument(usage); }
                if (key == "--bodies") { bodies = number; }
                else if (key == "--steps") { steps = number; }
                else { warmup = number; }
            }
        }
        if (!(seen & 1) || bodies == 0 || bodies > 1000000 || steps == 0 || steps > 10000 || warmup > 10000) {
            throw std::invalid_argument(usage);
        }
        coresim::World world;
        coresim::make_layout_world(world, bodies, order == "shuffled");
        coresim::LayoutExperiment experiment;
        for (unsigned tick = 0; tick < warmup; ++tick) { experiment.step(world, layout, 1.0F/120); }
        std::cout.imbue(std::locale::classic());
        std::cout << std::fixed << std::setprecision(9);
        std::cout << "schema,layout,order,build_type,compiler,bodies,step,warmup,gather_ms,kernel_ms,scatter_ms,total_ms,checksum\n";
        for (unsigned tick = 0; tick < steps; ++tick) {
            const auto timing = experiment.step(world, layout, 1.0F/120);
            // Consume the updated state outside timing; never benchmark dead stores.
            const double checksum = coresim::layout_checksum(world);
            std::cout << "1," << name << ',' << order << ',' << CORESIM_BUILD_TYPE << ',' << CORESIM_COMPILER
                      << ',' << bodies << ',' << tick << ',' << warmup << ',' << timing.gather_ms << ','
                      << timing.kernel_ms << ',' << timing.scatter_ms << ',' << timing.total_ms << ',' << checksum << '\n';
        }
        std::cout.flush();
        if (!std::cout) { throw std::runtime_error("Could not write layout CSV"); }
        return 0;
    } catch (const std::exception& error) { std::cerr << "CoreSim: " << error.what() << '\n'; return 1; }
}
