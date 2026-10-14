#include <coresim/gpu/VectorAdd.hpp>
#include "VectorAddInternal.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
namespace coresim::gpu {
void detail::validate_inputs(std::span<const float> a,std::span<const float> b) {
    if (a.size()!=b.size() || a.size()>std::numeric_limits<std::size_t>::max()/sizeof(float)) {
        throw std::invalid_argument("Vector lengths differ or byte count overflows");
    }
    for (std::size_t i=0;i<a.size();++i) {
        const double sum=static_cast<double>(a[i])+b[i];
        if (!std::isfinite(a[i]) || !std::isfinite(b[i]) || std::abs(sum)>std::numeric_limits<float>::max()) {
            throw std::invalid_argument("Vector addition requires finite inputs and representable sums");
        }
    }
}
std::vector<float> cpu_vector_add(std::span<const float> a,std::span<const float> b) {
    detail::validate_inputs(a,b);
    std::vector<float> result(a.size());
    for (std::size_t i=0;i<a.size();++i) { result[i]=a[i]+b[i]; }
    return result;
}
} // namespace coresim::gpu
