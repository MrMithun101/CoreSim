#include "CudaInternal.hpp"
#include <stdexcept>
namespace coresim::particles {
CudaStatus cuda_status() { return {false, {}, "CUDA was not built; configure CORESIM_ENABLE_CUDA=ON on Linux with NVCC"}; }
std::vector<Particle> detail::cuda_batch(std::span<const Particle>, Acceleration, std::size_t, int) {
    throw std::runtime_error(cuda_status().diagnostic);
}
}
