#include <coresim/particles/CudaParticles.hpp>
#include <coresim/particles/ParticleRunner.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace coresim::particles;

TEST_CASE("CUDA tolerance checks all finite scalars against the CPU reference") {
    std::vector<Particle> ref(2), actual(2);
    ref[0].x = actual[0].x = 1000;
    actual[0].x += .01F;
    actual[1].vz = .0001F;
    REQUIRE(compare_cuda(ref, actual).equal);
    actual[0].x += .02F;
    actual[1].z = .0003F;
    const auto failed = compare_cuda(ref, actual);
    REQUIRE_FALSE(failed.equal);
    REQUIRE(failed.mismatched_particles == 2);
    REQUIRE(failed.max_absolute_error > .02);
    for (float bad : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        actual[0].vx = bad;
        REQUIRE_FALSE(compare_cuda(actual, actual).equal);
        REQUIRE(std::isinf(compare_cuda(ref, actual).max_absolute_error));
    }
    REQUIRE_THROWS_AS(compare_cuda(ref, {}), std::invalid_argument);
}
TEST_CASE("CUDA batch rejects invalid inputs before device access") {
    auto p = initial_particles(1, 0);
    REQUIRE_THROWS_AS(integrate_cuda(p, default_acceleration, 10001), std::invalid_argument);
    REQUIRE_THROWS_AS(integrate_cuda(p, default_acceleration, 1, -1), std::invalid_argument);
    auto bad = std::numeric_limits<float>::infinity();
    REQUIRE_THROWS_AS(integrate_cuda(p, {bad,0,0}, 1), std::invalid_argument);
    p[0].y = bad;
    REQUIRE_THROWS_AS(integrate_cuda(p, default_acceleration, 0), std::invalid_argument);
}
TEST_CASE("CUDA requests are explicit and never fall back") {
    using Args = std::vector<std::string_view>;
    auto options = parse_options(Args{"--backend","cuda","--device","1"});
    REQUIRE(options.backend == Backend::cuda);
    REQUIRE(options.device == 1);
    for (auto bad : {Args{"--device","0"},Args{"--backend","cuda","--workers","4"},Args{"--cuda-info","--steps","3"},
                     Args{"--backend","cuda","--device","-1"},Args{"--backend","cuda","--device","2147483648"}}) {
        REQUIRE_THROWS_AS(parse_options(bad), std::invalid_argument);
    }
    auto status = cuda_status();
#ifdef CORESIM_TEST_CUDA
    REQUIRE(status.compiled);
#else
    REQUIRE_FALSE(status.compiled);
    REQUIRE(status.devices.empty());
    REQUIRE_FALSE(status.diagnostic.empty());
    REQUIRE_THROWS_AS(integrate_cuda({}, default_acceleration, 0), std::runtime_error);
    CudaPhases phases{-1,-1,-1};
    REQUIRE_THROWS_AS(integrate_cuda(initial_particles(8,1), default_acceleration, 2, 0, &phases), std::runtime_error);
    REQUIRE(phases.upload_ms == -1);
    REQUIRE(phases.kernel_ms == -1);
    REQUIRE(phases.download_ms == -1);
#endif
}
