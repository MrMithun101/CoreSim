#pragma once
#include <coresim/particles/CudaParticles.hpp>
namespace coresim::particles::detail {
std::vector<Particle> cuda_batch(std::span<const Particle>, Acceleration, std::size_t steps, int device, CudaPhases* phases);
}
