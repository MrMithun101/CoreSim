#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>
namespace coresim {
enum class SceneKind { collision_demo, stress_tower, sphere_rain, particle_storm };
struct SceneConfig {
    SceneKind kind = SceneKind::collision_demo;
    std::size_t objects = 100;
    std::uint32_t seed = 1;
};
[[nodiscard]] std::string_view scene_name(SceneKind kind);
// Zero objects selects the scene default. Explicit CLI zero is rejected by the parser.
[[nodiscard]] SceneConfig scene_config(std::string_view name, std::size_t objects = 0, std::uint32_t seed = 1);
void validate_scene_config(const SceneConfig& config);
[[nodiscard]] std::uint64_t scene_integer(std::string_view value, std::uint64_t maximum, bool allow_zero = false);
} // namespace coresim
