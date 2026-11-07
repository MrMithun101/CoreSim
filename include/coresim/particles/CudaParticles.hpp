#pragma once
#include <coresim/particles/ParticleSimulation.hpp>
#include <string>

namespace coresim::particles {
struct CudaDevice {
    int index, major, minor;
    std::string name;
};
struct CudaStatus {
    bool compiled;
    std::vector<CudaDevice> devices;
    std::string diagnostic;
    int driver_api{}, runtime{}, toolkit{};
};
[[nodiscard]] CudaStatus cuda_status();

struct CudaPhases { float upload_ms{}, kernel_ms{}, download_ms{}; };
// Optional event intervals are published only on batch success.
// Exclusive, synchronous batch. Input stays on the host, unchanged. No CPU fallback.
// A successful call returns all final particles; no partial batch is published.
[[nodiscard]] std::vector<Particle> integrate_cuda(std::span<const Particle> initial,
    Acceleration acceleration, std::size_t steps, int device = 0, CudaPhases* phases = nullptr);

inline constexpr double cuda_abs_tolerance = 2e-4;
inline constexpr double cuda_rel_tolerance = 2e-5;
// Per scalar: |actual-reference| <= abs_tol + rel_tol * |reference|; both must be finite.
[[nodiscard]] Comparison compare_cuda(std::span<const Particle> reference, std::span<const Particle> actual);
}
