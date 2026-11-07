#include <coresim/particles/CudaParticles.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cuda_runtime_api.h>
#include <cmath>
#include <limits>
#include <stdexcept>
using namespace coresim::particles;

TEST_CASE("Real CUDA particles match the serial reference and preserve caller state") {
    auto status = cuda_status();
    INFO(status.diagnostic);
    REQUIRE(status.compiled);
    REQUIRE(status.diagnostic.empty());
    REQUIRE_FALSE(status.devices.empty());
    int original = -1;
    REQUIRE(cudaGetDevice(&original) == cudaSuccess);
    for (const auto& device : status.devices) {
        for (auto count : {0U,1U,255U,256U,257U,1000U,10000U,100000U}) {
            auto input = initial_particles(count, 7);
            auto saved = input;
            const Acceleration acceleration{.5F,-9.81F,1.5F};
            ParticleSimulation cpu(input, acceleration);
            constexpr std::size_t steps=120;
            for (std::size_t i=0; i<steps; ++i) { (void)cpu.step_serial(); }
            REQUIRE(integrate_cuda(input, acceleration, 0, device.index) == input);
            for (int repeat=0; repeat<2; ++repeat) {
                CudaPhases phases{-1,-1,-1};
                const auto actual=integrate_cuda(input, acceleration, steps, device.index, &phases);
                for (float t : {phases.upload_ms, phases.kernel_ms, phases.download_ms}) {
                    REQUIRE(std::isfinite(t));
                    REQUIRE(t >= 0);
                    if (input.empty()) { REQUIRE(t == 0); }
                }
                REQUIRE(compare_cuda(cpu.state(), actual).equal);
            }
            REQUIRE(input == saved);
            int current = -1;
            REQUIRE(cudaGetDevice(&current) == cudaSuccess);
            REQUIRE(current == original);
        }
        auto overflow = initial_particles(257,1);
        overflow.back().x = overflow.back().vx = std::numeric_limits<float>::max();
        auto saved = overflow;
        REQUIRE_THROWS_AS(integrate_cuda(overflow, default_acceleration, 2, device.index), std::overflow_error);
        REQUIRE(saved == overflow);
        int current=-1;
        REQUIRE(cudaGetDevice(&current)==cudaSuccess);
        REQUIRE(current==original);
    }
    REQUIRE_THROWS_AS(integrate_cuda({}, default_acceleration, 0, static_cast<int>(status.devices.size())), std::invalid_argument);
}
