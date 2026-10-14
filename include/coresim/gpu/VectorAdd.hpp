#pragma once
#include <span>
#include <string>
#include <vector>
namespace coresim::gpu {
struct DeviceInfo {
    int index{};
    std::string name;
    std::string architecture;
};
struct HipStatus {
    bool compiled{};
    std::vector<DeviceInfo> devices;
    std::string diagnostic;
};
// The inspector reports runtime/driver failures as diagnostics, not as usable devices.
[[nodiscard]] HipStatus hip_status();
// Inputs must have equal lengths and finite, representable sums. Empty input is valid.
[[nodiscard]] std::vector<float> cpu_vector_add(std::span<const float> a, std::span<const float> b);
// Synchronous default-stream operation. Never falls back to CPU. Restores the caller's
// current device; caller must not reset the device concurrently with this operation.
[[nodiscard]] std::vector<float> hip_vector_add(std::span<const float> a, std::span<const float> b, int device = 0);
} // namespace coresim::gpu
