#pragma once
#include <coresim/scene/SceneConfig.hpp>
#include <coresim/studio/SandboxEditor.hpp>
#include <filesystem>
namespace coresim {
struct SceneDocument {
    SceneKind kind{SceneKind::sandbox};
    std::vector<ObjectSpec> objects;
    int projectile{-1};
    glm::vec3 launch{16, 0, 0};
    Camera camera;
};
SceneDocument read_scene_file(const std::filesystem::path& path);
void write_scene_file(const std::filesystem::path& path, SceneKind kind,
                      const SandboxEditor& editor, const Camera& camera);
} // namespace coresim
