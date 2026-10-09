#include <coresim/benchmark/CollisionBenchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace coresim;
TEST_CASE("Benchmark CLI rejects ambiguous malformed or unbounded workloads") {
    const std::array<std::string_view, 8> valid{
        "--bodies", "93", "--benchmark", "collision-naive", "--warmup", "0", "--steps", "2"};
    const auto options = parse_benchmark_options(valid);
    REQUIRE(options.bodies == 93);
    REQUIRE(options.steps == 2);
    REQUIRE(options.warmup == 0);
    const std::vector<std::vector<std::string_view>> invalid{
        {}, {"--benchmark"}, {"--benchmark", "unknown"}, {"--bodies", "10"},
        {"--benchmark", "collision-naive", "--bodies", "0"},
        {"--benchmark", "collision-naive", "--bodies", "-1"},
        {"--benchmark", "collision-naive", "--bodies", "100001"},
        {"--benchmark", "collision-naive", "--steps", "0"},
        {"--benchmark", "collision-naive", "--steps", "2x"},
        {"--benchmark", "collision-naive", "--warmup", "10001"},
        {"--benchmark", "collision-naive", "--warmup", "4294967296"},
        {"--benchmark", "collision-naive", "--bodies", "1", "--bodies", "2"},
        {"--benchmark", "collision-naive", "--frames", "2"}};
    for (const auto& arguments : invalid) { REQUIRE_THROWS_AS(parse_benchmark_options(arguments), std::invalid_argument); }
}
TEST_CASE("Benchmark CSV contains one complete measurable row per requested tick") {
    std::ostringstream output;
    run_collision_benchmark({93, 2, 1}, output);
    std::istringstream input(output.str());
    std::string line;
    REQUIRE(static_cast<bool>(std::getline(input, line)));
    REQUIRE(line.find("physics_step_ms,total_frame_ms") != std::string::npos);
    for (int tick = 0; tick < 2; ++tick) {
        REQUIRE(static_cast<bool>(std::getline(input, line)));
        std::istringstream row(line);
        std::vector<std::string> fields;
        std::string field;
        while (std::getline(row, field, ',')) { fields.push_back(field); }
        REQUIRE(fields.size() == 18);
        REQUIRE(fields[0] == "1");
        REQUIRE(fields[1] == "collision-naive");
        REQUIRE(fields[2] == "separated-spheres");
        REQUIRE(fields[5] == "93");
        REQUIRE(fields[6] == std::to_string(tick));
        REQUIRE(fields[7] == "1");
        REQUIRE(fields[9] == "4278");
        REQUIRE(fields[10] == "4278");
        REQUIRE(fields[11] == "0");
        REQUIRE(fields[12] == "0");
        for (std::size_t i = 13; i < fields.size(); ++i) {
            REQUIRE(std::isfinite(std::stod(fields[i])));
            REQUIRE(std::stod(fields[i]) >= 0);
        }
        REQUIRE(std::stod(fields[17]) >= std::stod(fields[16]));
    }
    REQUIRE_FALSE(static_cast<bool>(std::getline(input, line)));
    std::ostringstream broken;
    broken.setstate(std::ios::badbit);
    REQUIRE_THROWS_AS(run_collision_benchmark({1, 1, 0}, broken), std::runtime_error);
}
