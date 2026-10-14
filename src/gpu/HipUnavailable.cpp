#include <coresim/gpu/VectorAdd.hpp>
#include "VectorAddInternal.hpp"
#include <stdexcept>
namespace coresim::gpu {
HipStatus hip_status() { return {false,{},"HIP was not built; configure CORESIM_ENABLE_HIP=ON with a supported Linux ROCm toolchain"}; }
std::vector<float> hip_vector_add(std::span<const float> a,std::span<const float> b,int) {
    detail::validate_inputs(a,b);
    throw std::runtime_error(hip_status().diagnostic);
}
} // namespace coresim::gpu
