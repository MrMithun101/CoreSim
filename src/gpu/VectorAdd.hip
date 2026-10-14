#include <coresim/gpu/VectorAdd.hpp>
#include "VectorAddInternal.hpp"
#include <hip/hip_runtime.h>
#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <utility>

namespace coresim::gpu {
namespace {
void check(hipError_t error,const char* operation) {
    if (error!=hipSuccess) { throw std::runtime_error(std::string(operation)+": "+hipGetErrorString(error)); }
}
void cleanup(hipError_t error,const char* operation) noexcept {
    if (error!=hipSuccess) { std::fprintf(stderr,"CoreSim HIP cleanup %s: %s\n",operation,hipGetErrorString(error)); }
}
// Private owners never outlive the selected-device scope. Destructors cannot throw
// while unwinding; normal completion uses checked release/restore calls instead.
class DeviceScope {
public:
    explicit DeviceScope(int device) {
        check(hipGetDevice(&previous_),"hipGetDevice");
        check(hipSetDevice(device),"hipSetDevice");
    }
    ~DeviceScope() { if (active_) { cleanup(hipSetDevice(previous_),"restore device"); } }
    DeviceScope(const DeviceScope&)=delete;
    DeviceScope& operator=(const DeviceScope&)=delete;
    void restore() { check(hipSetDevice(previous_),"restore device"); active_=false; }
private:
    int previous_{};
    bool active_=true;
};
class DeviceBuffer {
public:
    explicit DeviceBuffer(std::size_t bytes) { check(hipMalloc(&data_,bytes),"hipMalloc"); }
    ~DeviceBuffer() { if (data_) { cleanup(hipFree(data_),"hipFree"); } }
    DeviceBuffer(const DeviceBuffer&)=delete;
    DeviceBuffer& operator=(const DeviceBuffer&)=delete;
    DeviceBuffer(DeviceBuffer&& other) noexcept : data_(std::exchange(other.data_,nullptr)) {}
    DeviceBuffer& operator=(DeviceBuffer&&)=delete;
    float* data() const { return static_cast<float*>(data_); }
    void release() {
        if (data_) { check(hipFree(data_),"hipFree"); data_=nullptr; }
    }
private:
    void* data_{};
};
__global__ void add_vectors(const float* a,const float* b,float* output,std::size_t count) {
    const auto index=static_cast<std::size_t>(blockIdx.x)*blockDim.x+threadIdx.x;
    if (index<count) { output[index]=a[index]+b[index]; }
}
}
HipStatus hip_status() {
    HipStatus status{true,{}, {}};
    int count=0;
    const auto error=hipGetDeviceCount(&count);
    if (error!=hipSuccess) { status.diagnostic=std::string("hipGetDeviceCount: ")+hipGetErrorString(error); return status; }
    for (int i=0;i<count;++i) {
        hipDeviceProp_t properties{};
        const auto result=hipGetDeviceProperties(&properties,i);
        if (result!=hipSuccess) {
            status.devices.clear(); status.diagnostic=std::string("hipGetDeviceProperties: ")+hipGetErrorString(result); return status;
        }
        status.devices.push_back({i,properties.name,properties.gcnArchName});
    }
    if (status.devices.empty()) { status.diagnostic="HIP runtime reports no devices"; }
    return status;
}
std::vector<float> hip_vector_add(std::span<const float> a,std::span<const float> b,int device) {
    detail::validate_inputs(a,b);
    const auto status=hip_status();
    if (!status.diagnostic.empty()) { throw std::runtime_error(status.diagnostic); }
    if (device<0 || static_cast<std::size_t>(device)>=status.devices.size()) { throw std::invalid_argument("HIP device index is unavailable"); }
    DeviceScope scope(device);
    std::vector<float> output(a.size());
    if (!a.empty()) {
        hipDeviceProp_t properties{};
        check(hipGetDeviceProperties(&properties,device),"hipGetDeviceProperties");
        if (properties.maxThreadsPerBlock<=0) { throw std::runtime_error("HIP device has no usable block size"); }
        const auto block=static_cast<unsigned int>(std::min(256,properties.maxThreadsPerBlock));
        if (block==0) { throw std::runtime_error("HIP device has no usable block size"); }
        const auto grid=a.size()/block+(a.size()%block!=0 ? 1U : 0U);
        if (properties.maxGridSize[0]<=0 || grid>static_cast<std::size_t>(properties.maxGridSize[0])) {
            throw std::invalid_argument("Vector exceeds device grid capacity");
        }
        const auto bytes=a.size()*sizeof(float);
        DeviceBuffer da(bytes), db(bytes), dc(bytes);
        check(hipMemcpy(da.data(),a.data(),bytes,hipMemcpyHostToDevice),"copy input A to device");
        check(hipMemcpy(db.data(),b.data(),bytes,hipMemcpyHostToDevice),"copy input B to device");
        hipLaunchKernelGGL(add_vectors,dim3(static_cast<unsigned int>(grid)),dim3(block),0,0,da.data(),db.data(),dc.data(),a.size());
        check(hipGetLastError(),"vector-add kernel launch");
        // Launch success does not prove successful execution. Surface asynchronous faults
        // before publishing host results or releasing device storage.
        check(hipDeviceSynchronize(),"vector-add kernel execution");
        check(hipMemcpy(output.data(),dc.data(),bytes,hipMemcpyDeviceToHost),"copy output to host");
        dc.release(); db.release(); da.release();
    }
    scope.restore();
    return output;
}
} // namespace coresim::gpu
