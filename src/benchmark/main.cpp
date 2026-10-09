#include <coresim/benchmark/CollisionBenchmark.hpp>
#include <exception>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
    try {
        std::vector<std::string_view> arguments(argv + 1, argv + argc);
        if (arguments.size() == 1 && arguments.front() == "--help") {
            std::cout << "Usage: coresim_benchmark " << coresim::benchmark_usage;
            return 0;
        }
        coresim::run_collision_benchmark(coresim::parse_benchmark_options(arguments), std::cout);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "CoreSim: " << error.what() << '\n';
        return 1;
    }
}
