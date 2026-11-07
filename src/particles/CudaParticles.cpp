#include "CudaInternal.hpp"
#include "ParticleKernel.hpp"
#include <limits>

namespace coresim::particles {
std::vector<Particle> integrate_cuda(std::span<const Particle> input, Acceleration acceleration,
                                     std::size_t steps, int device) {
    detail::validate(input, acceleration);
    if (device < 0 || steps > 10000) { throw std::invalid_argument("Invalid CUDA device or tick count"); }
    return detail::cuda_batch(input, acceleration, steps, device);
}

Comparison compare_cuda(std::span<const Particle> reference, std::span<const Particle> actual) {
    if (reference.size() != actual.size()) { throw std::invalid_argument("Particle comparison counts differ"); }
    const auto components = [](const Particle& p) { return std::array{p.x,p.y,p.z,p.vx,p.vy,p.vz}; };
    Comparison result;
    for (std::size_t i = 0; i < reference.size(); ++i) {
        auto ref = components(reference[i]), value = components(actual[i]);
        bool mismatch = false;
        for (std::size_t j = 0; j < ref.size(); ++j) {
            const double expected = ref[j];
            const auto error = std::isfinite(expected) && std::isfinite(value[j])
                ? std::abs(expected - value[j]) : std::numeric_limits<double>::infinity();
            result.max_absolute_error = std::max(result.max_absolute_error, error);
            if (!std::isfinite(error) || error > cuda_abs_tolerance + cuda_rel_tolerance * std::abs(expected)) {
                mismatch = true;
            }
        }
        if (mismatch) { ++result.mismatched_particles; }
    }
    result.equal = result.mismatched_particles == 0;
    return result;
}
}
