#include <coresim/gpu/Demo.hpp>
#include <exception>
#include <iostream>
#include <vector>
int main(int argc,char** argv) {
    try {
        const std::vector<std::string_view> arguments(argv+1,argv+argc);
        coresim::gpu::run_demo(coresim::gpu::parse_demo_options(arguments),std::cout);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "CoreSim GPU demo: " << error.what() << '\n';
        return 1;
    }
}
