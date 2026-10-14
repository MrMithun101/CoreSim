#pragma once
#include <span>
namespace coresim::gpu::detail {
void validate_inputs(std::span<const float> a, std::span<const float> b);
}
