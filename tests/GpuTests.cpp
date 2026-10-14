#include <coresim/gpu/VectorAdd.hpp>
#include <coresim/gpu/Demo.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <sstream>
#include <stdexcept>
using namespace coresim::gpu;
TEST_CASE("CPU vector addition handles block boundaries and preserves inputs") {
    for (const std::size_t count:{0U,1U,255U,256U,257U,1025U,1048579U}) {
        std::vector<float> a(count),b(count),expected(count);
        for (std::size_t i=0;i<count;++i) {
            a[i]=static_cast<float>(static_cast<int>(i%71)-35)*0.25F;
            b[i]=static_cast<float>(static_cast<int>(i%23)-11)*0.5F;
            expected[i]=a[i]+b[i];
        }
        const auto saved_a=a,saved_b=b;
        REQUIRE(cpu_vector_add(a,b)==expected);
        REQUIRE(a==saved_a);
        REQUIRE(b==saved_b);
    }
}
TEST_CASE("Vector addition rejects mismatched and nonfinite data before backend access") {
    const std::vector<float> zero{0},one{1},empty{},large{std::numeric_limits<float>::max()};
    REQUIRE_THROWS_AS(cpu_vector_add(zero,empty),std::invalid_argument);
    REQUIRE_THROWS_AS(hip_vector_add(zero,empty),std::invalid_argument);
    REQUIRE_THROWS_AS(cpu_vector_add(large,large),std::invalid_argument);
    REQUIRE(cpu_vector_add(large,zero)==large);
    for (const float value:{std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        const std::vector<float> invalid{value};
        REQUIRE_THROWS_AS(cpu_vector_add(invalid,one),std::invalid_argument);
        REQUIRE_THROWS_AS(cpu_vector_add(one,invalid),std::invalid_argument);
        REQUIRE_THROWS_AS(hip_vector_add(invalid,one),std::invalid_argument);
    }
}
TEST_CASE("GPU demo validates options and verifies the CPU workload") {
    const std::vector<std::string_view> args{"--gpu-demo","vector-add","--elements","257","--backend","cpu"};
    const auto options=parse_demo_options(args);
    REQUIRE(options.elements==257);
    REQUIRE_FALSE(options.hip);
    std::ostringstream out;
    run_demo(options,out);
    REQUIRE(out.str().find("backend=cpu elements=257 verified=true")!=std::string::npos);
    for (const auto& invalid:std::vector<std::vector<std::string_view>>{
        {"--elements","-1"},{"--elements","2x"},{"--elements","16777217"},{"--elements","18446744073709551616"},
        {"--elements",""},{"--elements"},{"--backend","auto"},{"--gpu-demo","particles"},
        {"--elements","1","--elements","2"},{"--unknown"},{"--backend","hip","--device","-1"},
        {"--backend","hip","--device","2147483648"},{"--device","0"},{"--benchmark","collision-spatial"},
        {"--help","--unknown"}}) {
        REQUIRE_THROWS_AS(parse_demo_options(invalid),std::invalid_argument);
    }
    const std::vector<std::string_view> empty_args{"--elements","0"};
    std::ostringstream empty_out;
    run_demo(parse_demo_options(empty_args),empty_out);
    REQUIRE(empty_out.str().find("elements=0 verified=true checksum=0")!=std::string::npos);
}
#ifndef CORESIM_TEST_HIP_COMPILED
TEST_CASE("Unavailable HIP backend never silently falls back") {
    const auto status=hip_status();
    REQUIRE_FALSE(status.compiled);
    REQUIRE(status.devices.empty());
    REQUIRE_FALSE(status.diagnostic.empty());
    const std::vector<float> input{1};
    REQUIRE_THROWS_AS(hip_vector_add(input,input),std::runtime_error);
    REQUIRE_THROWS_AS(hip_vector_add({},{}),std::runtime_error);
}
#endif
