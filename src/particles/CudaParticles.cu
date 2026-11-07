#include "CudaInternal.hpp"
#include <cuda_runtime.h>
#include <algorithm>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace coresim::particles {
namespace {
static_assert(sizeof(Particle) == 24 && std::is_trivially_copyable_v<Particle>);
void check(cudaError_t error, const char* operation) {
    if (error != cudaSuccess) { throw std::runtime_error(std::string(operation) + ": " + cudaGetErrorString(error)); }
}
void cleanup(cudaError_t error, const char* operation) noexcept {
    if (error != cudaSuccess) { std::fprintf(stderr, "CoreSim CUDA cleanup (%s): %s\n", operation, cudaGetErrorString(error)); }
}
struct DeviceScope {
    int previous;
    bool changed;
    explicit DeviceScope(int selected) {
        check(cudaGetDevice(&previous), "get current CUDA device");
        changed = previous != selected;
        if (changed) { check(cudaSetDevice(selected), "select CUDA device"); }
    }
    DeviceScope(const DeviceScope&) = delete;
    void restore() {
        if (changed) { check(cudaSetDevice(previous), "restore CUDA device"); changed = false; }
    }
    ~DeviceScope() { if (changed) { cleanup(cudaSetDevice(previous), "restore device"); } }
};
struct Stream {
    cudaStream_t value{};
    Stream() { check(cudaStreamCreateWithFlags(&value, cudaStreamNonBlocking), "create particle stream"); }
    Stream(const Stream&) = delete;
    ~Stream() { cleanup(cudaStreamDestroy(value), "destroy stream"); }
};
struct Event {
    cudaEvent_t value{};
    Event() { check(cudaEventCreate(&value), "create timing event"); }
    Event(const Event&) = delete;
    ~Event() { cleanup(cudaEventDestroy(value), "destroy timing event"); }
};
struct Events {
    std::array<Event,6> marks;
    float elapsed(unsigned begin) const {
        float ms{};
        check(cudaEventElapsedTime(&ms, marks[begin].value, marks[begin+1].value), "measure CUDA interval");
        return ms;
    }
};
template<class T> struct Buffer {
    T* ptr{};
    explicit Buffer(std::size_t count) { check(cudaMalloc(&ptr, count * sizeof(T)), "allocate particle batch storage"); }
    Buffer(const Buffer&) = delete;
    ~Buffer() { cleanup(cudaFree(ptr), "free device storage"); }
};
__global__ void integrate_tick(Particle* state, std::size_t count, float ax, float ay, float az, float dt, int* invalid) {
    const auto i = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (i >= count) { return; }
    auto p = state[i];
    p.vx += ax * dt; p.vy += ay * dt; p.vz += az * dt;
    p.x += p.vx * dt; p.y += p.vy * dt; p.z += p.vz * dt;
    if (!(isfinite(p.x) && isfinite(p.y) && isfinite(p.z) &&
          isfinite(p.vx) && isfinite(p.vy) && isfinite(p.vz))) {
        atomicExch(invalid, 1);
        return;
    }
    state[i] = p;
}
}
CudaStatus cuda_status() {
    CudaStatus status{true, {}, {}};
    int count{};
    auto error = cudaGetDeviceCount(&count);
    if (error != cudaSuccess) { status.diagnostic = std::string("cudaGetDeviceCount: ") + cudaGetErrorString(error); return status; }
    status.toolkit = CUDART_VERSION;
    error = cudaDriverGetVersion(&status.driver_api);
    if (error == cudaSuccess) { error = cudaRuntimeGetVersion(&status.runtime); }
    if (error != cudaSuccess) { status.diagnostic = std::string("CUDA version query: ") + cudaGetErrorString(error); return status; }
    for (int i = 0; i < count; ++i) {
        cudaDeviceProp props{};
        error = cudaGetDeviceProperties(&props, i);
        if (error != cudaSuccess) {
            status.devices.clear();
            status.diagnostic = std::string("cudaGetDeviceProperties: ") + cudaGetErrorString(error);
            return status;
        }
        status.devices.push_back({i, props.major, props.minor, props.name});
    }
    if (!count) { status.diagnostic = "CUDA runtime reports no devices"; }
    return status;
}
std::vector<Particle> detail::cuda_batch(std::span<const Particle> initial, Acceleration a, std::size_t steps, int device, CudaPhases* phases) {
    const auto status = cuda_status();
    if (!status.diagnostic.empty()) { throw std::runtime_error(status.diagnostic); }
    if (static_cast<std::size_t>(device) >= status.devices.size()) { throw std::invalid_argument("CUDA device index unavailable"); }
    CudaPhases measured;
    DeviceScope scope(device);
    std::vector<Particle> result(initial.begin(), initial.end());
    if (!initial.empty() && steps != 0) {
        cudaDeviceProp props{};
        check(cudaGetDeviceProperties(&props, device), "query CUDA launch limits");
        const int threads = std::min({256, props.maxThreadsDim[0], props.maxThreadsPerBlock});
        if (threads <= 0) { throw std::runtime_error("No usable CUDA block size"); }
        const auto blocks = (initial.size() + static_cast<unsigned>(threads) - 1) / static_cast<unsigned>(threads);
        if (props.maxGridSize[0] <= 0 || blocks > static_cast<std::size_t>(props.maxGridSize[0])) {
            throw std::invalid_argument("Particle workload exceeds CUDA grid limits");
        }
        // No host result is exposed until the whole batch succeeds. Particles are independent.
        Stream stream;
        Buffer<Particle> state(initial.size());
        Buffer<int> invalid(1);
        int flag{};
        auto events = phases ? std::make_unique<Events>() : nullptr;
        auto mark = [&](unsigned index) {
            if (events) { check(cudaEventRecord(events->marks[index].value, stream.value), "record CUDA event"); }
        };
        mark(0);
        check(cudaMemcpyAsync(state.ptr, initial.data(), initial.size_bytes(), cudaMemcpyHostToDevice, stream.value), "upload particles");
        mark(1);
        check(cudaMemsetAsync(invalid.ptr, 0, sizeof(int), stream.value), "clear overflow flag");
        mark(2);
        for (std::size_t i = 0; i < steps; ++i) {
            integrate_tick<<<static_cast<unsigned>(blocks), threads, 0, stream.value>>>(
                state.ptr, initial.size(), a[0], a[1], a[2], timestep, invalid.ptr);
            check(cudaGetLastError(), "launch CUDA particle tick");
        }
        mark(3);
        check(cudaStreamSynchronize(stream.value), "complete CUDA particle ticks");
        check(cudaMemcpy(&flag, invalid.ptr, sizeof(int), cudaMemcpyDeviceToHost), "read overflow flag");
        if (flag) { throw std::overflow_error("Non-finite CUDA particle integration result"); }
        mark(4);
        check(cudaMemcpyAsync(result.data(), state.ptr, initial.size_bytes(), cudaMemcpyDeviceToHost, stream.value), "download particles");
        mark(5);
        check(cudaStreamSynchronize(stream.value), "complete particle download");
        if (events) { measured = {events->elapsed(0), events->elapsed(2), events->elapsed(4)}; }
    }
    scope.restore();
    if (phases) { *phases = measured; }
    return result;
}
}
