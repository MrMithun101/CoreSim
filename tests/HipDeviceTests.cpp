#include <coresim/gpu/VectorAdd.hpp>
#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
using namespace coresim::gpu;
TEST_CASE("Real HIP kernel matches CPU across launch boundaries and repeated allocations") {
    const auto status=hip_status();
    INFO(status.diagnostic);
    REQUIRE(status.compiled);
    REQUIRE(status.diagnostic.empty());
    REQUIRE_FALSE(status.devices.empty());
    for (const auto& device:status.devices) {
        for (const std::size_t count:{0U,1U,255U,256U,257U,1025U,1048579U}) {
            std::vector<float> a(count),b(count);
            for (std::size_t i=0;i<count;++i) {
                a[i]=static_cast<float>(static_cast<int>(i%257)-128)*0.25F;
                b[i]=static_cast<float>(static_cast<int>(i%127)-63)*0.5F;
            }
            const auto saved_a=a,saved_b=b;
            const auto reference=cpu_vector_add(a,b);
            for (int repeat=0;repeat<3;++repeat) { REQUIRE(hip_vector_add(a,b,device.index)==reference); }
            REQUIRE(a==saved_a);
            REQUIRE(b==saved_b);
        }
    }
    REQUIRE_THROWS_AS(hip_vector_add({}, {}, -1),std::invalid_argument);
    REQUIRE_THROWS_AS(hip_vector_add({}, {}, static_cast<int>(status.devices.size())),std::invalid_argument);
}
