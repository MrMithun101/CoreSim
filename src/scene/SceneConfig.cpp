#include <coresim/scene/SceneConfig.hpp>
#include <charconv>
#include <stdexcept>
namespace coresim {
std::string_view scene_name(SceneKind kind) {
    switch (kind) {
    case SceneKind::collision_demo: return "collision-demo";
    case SceneKind::stress_tower: return "stress-tower";
    case SceneKind::sphere_rain: return "sphere-rain";
    case SceneKind::particle_storm: return "particle-storm";
    }
    throw std::invalid_argument("Unknown scene kind");
}
void validate_scene_config(const SceneConfig& c) {
    (void)scene_name(c.kind);
    const std::size_t limit = c.kind == SceneKind::stress_tower ? 1000 : c.kind == SceneKind::sphere_rain ? 25000 : 100000;
    if (c.objects == 0 || c.objects > limit || (c.kind == SceneKind::collision_demo && c.objects != 100)) {
        throw std::invalid_argument("Object count: collision-demo requires 100; tower supports 1..1000, rain 1..25000, particles 1..100000");
    }
}
SceneConfig scene_config(std::string_view name, std::size_t objects, std::uint32_t seed) {
    SceneConfig c;
    if (name == "collision-demo" || name == "sandbox") { c.kind=SceneKind::collision_demo; c.objects=100; }
    else if (name == "stress-tower") { c.kind=SceneKind::stress_tower; c.objects=256; }
    else if (name == "sphere-rain") { c.kind=SceneKind::sphere_rain; c.objects=2500; }
    else if (name == "particle-storm") { c.kind=SceneKind::particle_storm; c.objects=25000; }
    else { throw std::invalid_argument("Unknown scene name"); }
    if (objects) { c.objects=objects; }
    c.seed=seed;
    validate_scene_config(c);
    return c;
}
std::uint64_t scene_integer(std::string_view value, std::uint64_t maximum, bool allow_zero) {
    std::uint64_t result{};
    const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),result);
    if (error != std::errc{} || end != value.data()+value.size() || result>maximum || (!allow_zero && result==0)) {
        throw std::invalid_argument("Invalid scene numeric option");
    }
    return result;
}
} // namespace coresim
