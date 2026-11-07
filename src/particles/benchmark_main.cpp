#include <coresim/particles/ParticleBenchmark.hpp>
#include <exception>
#include <iostream>
#include <vector>
int main(int argc,char** argv) {
    try {
        const std::vector<std::string_view> arguments(argv+1,argv+argc);
        const auto options=coresim::particles::parse_benchmark_options(arguments);
        if (options.help) { std::cout << coresim::particles::benchmark_usage; return 0; }
        coresim::particles::write_benchmark(coresim::particles::benchmark(options),std::cout);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "CoreSim particle benchmark: " << error.what() << '\n';
        return 1;
    }
}
