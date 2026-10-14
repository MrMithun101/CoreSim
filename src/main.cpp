#include <coresim/core/Application.hpp>
#include <coresim/gpu/Demo.hpp>

#include <coresim/benchmark/CollisionBenchmark.hpp>
#include <algorithm>
#include <vector>
#include <set>
#include <limits>
#include <charconv>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>

int main(int argc, char** argv) {
    try {
        const std::vector<std::string_view> arguments(argv + 1, argv + argc);
        if (std::find(arguments.begin(), arguments.end(), "--gpu-demo") != arguments.end()) {
            coresim::gpu::run_demo(coresim::gpu::parse_demo_options(arguments), std::cout);
            return 0;
        }
        if (std::find(arguments.begin(), arguments.end(), "--benchmark") != arguments.end()) {
            coresim::run_collision_benchmark(coresim::parse_benchmark_options(arguments), std::cout);
            return 0;
        }
        std::uint64_t frame_limit = 0;
        std::filesystem::path shader_directory = CORESIM_SHADER_DIR;
        std::string_view scene="collision-demo";
        std::size_t objects=0;
        std::uint32_t seed=1;
        std::set<std::string_view> seen;
        constexpr auto usage = "Usage: coresim [--frames positive-integer] [--shader-dir path] [--scene collision-demo|stress-tower|sphere-rain|particle-storm] [--objects count] [--seed uint32]\n";
        for (int i = 1; i < argc; ++i) {
            const std::string_view option(argv[i]);
            if (option == "--help") {
                std::cout << "       coresim " << coresim::gpu::demo_usage << usage << "       coresim " << coresim::benchmark_usage << "WASD move, Q/E down/up, hold right mouse to look. R resets physics. Escape closes.\n";
                return 0;
            }
            if (!seen.insert(option).second || (option != "--frames" && option != "--shader-dir" && option != "--scene" && option != "--objects" && option != "--seed") || i + 1 >= argc) {
                throw std::invalid_argument(usage);
            }
            const std::string_view value(argv[++i]);
            if (option == "--scene") { scene=value; }
            else if (option == "--objects") { objects=static_cast<std::size_t>(coresim::scene_integer(value,100000)); }
            else if (option == "--seed") { seed=static_cast<std::uint32_t>(coresim::scene_integer(value,std::numeric_limits<std::uint32_t>::max(),true)); }
            else if (option == "--shader-dir") {
                if (value.empty()) {
                    throw std::invalid_argument("--shader-dir requires a nonempty path");
                }
                shader_directory = value;
            } else {
                const auto [end, error] =
                    std::from_chars(value.data(), value.data() + value.size(), frame_limit);
                if (error != std::errc{} || end != value.data() + value.size() || frame_limit == 0) {
                    throw std::invalid_argument("--frames requires a positive integer");
                }
            }
        }
        coresim::Application application(shader_directory,coresim::scene_config(scene,objects,seed));
        return application.run(frame_limit);
    } catch (const std::exception& error) {
        std::cerr << "CoreSim: " << error.what() << '\n';
        return 1;
    }
}
