#include <coresim/gpu/Demo.hpp>
#include <coresim/gpu/VectorAdd.hpp>
#include <charconv>
#include <limits>
#include <ostream>
#include <set>
#include <stdexcept>
#include <string>
namespace coresim::gpu {
namespace {
constexpr std::size_t max_elements=16777216;
template<class T> T integer(std::string_view value,T maximum) {
    T result{};
    const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),result);
    if (error!=std::errc{} || end!=value.data()+value.size() || result<0 || result>maximum) {
        throw std::invalid_argument("Invalid GPU demo integer: "+std::string(value));
    }
    return result;
}
}
DemoOptions parse_demo_options(std::span<const std::string_view> arguments) {
    DemoOptions options;
    std::set<std::string_view> seen;
    for (std::size_t i=0;i<arguments.size();++i) {
        const auto key=arguments[i];
        if (!seen.insert(key).second) { throw std::invalid_argument("Duplicate GPU demo option"); }
        if (key=="--help") { options.help=true; continue; }
        if (key=="--info") { options.info=true; continue; }
        if (key!="--gpu-demo" && key!="--backend" && key!="--elements" && key!="--device") {
            throw std::invalid_argument("Unknown GPU demo option: "+std::string(key));
        }
        if (++i==arguments.size()) { throw std::invalid_argument("Missing GPU demo value"); }
        const auto value=arguments[i];
        if (key=="--gpu-demo" && value!="vector-add") { throw std::invalid_argument("Only vector-add is supported"); }
        if (key=="--backend") {
            if (value!="cpu" && value!="hip") { throw std::invalid_argument("Backend must be cpu or hip"); }
            options.hip=value=="hip";
        }
        if (key=="--elements") { options.elements=integer<std::size_t>(value,max_elements); }
        if (key=="--device") { options.device=integer<int>(value,std::numeric_limits<int>::max()); }
    }
    if (seen.contains("--device") && !options.hip && !options.info) { throw std::invalid_argument("--device requires HIP or --info"); }
    return options;
}
void run_demo(const DemoOptions& options,std::ostream& output) {
    if (options.help) { output << demo_usage; return; }
    if (options.info) {
        const auto status=hip_status();
        output << "hip_compiled=" << (status.compiled ? "true" : "false") << " devices=" << status.devices.size() << '\n';
        for (const auto& device:status.devices) { output << "device=" << device.index << " name=" << device.name << " architecture=" << device.architecture << '\n'; }
        if (!status.diagnostic.empty()) { output << "diagnostic=" << status.diagnostic << '\n'; }
        return;
    }
    if (options.elements>max_elements || options.device<0) { throw std::invalid_argument("GPU demo options out of range"); }
    std::vector<float> a(options.elements), b(options.elements);
    for (std::size_t i=0;i<a.size();++i) {
        a[i]=static_cast<float>(static_cast<int>(i%257)-128)*0.25F;
        b[i]=static_cast<float>(static_cast<int>(i%127)-63)*0.5F;
    }
    const auto reference=cpu_vector_add(a,b);
    const auto result=options.hip ? hip_vector_add(a,b,options.device) : cpu_vector_add(a,b);
    if (result!=reference) { throw std::runtime_error("Vector-add CPU reference verification failed"); }
    double checksum=0;
    for (const float value:result) { checksum+=value; }
    output << "workload=vector-add backend=" << (options.hip ? "hip" : "cpu") << " elements=" << result.size()
           << " verified=true checksum=" << checksum << '\n';
}
}
