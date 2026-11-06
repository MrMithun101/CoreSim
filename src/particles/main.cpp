#include <coresim/particles/ParticleRunner.hpp>
#include <exception>
#include <iostream>
#include <vector>
int main(int argc, char** argv) {
    try {
        const std::vector<std::string_view> arguments(argv + 1, argv + argc);
        coresim::particles::run(coresim::particles::parse_options(arguments), std::cout);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "CoreSim particles: " << error.what() << '\n';
        return 1;
    }
}
