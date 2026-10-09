#pragma once

#include <coresim/renderer/Mesh.hpp>
#include <coresim/renderer/Shader.hpp>
#include <filesystem>
#include <glm/mat4x4.hpp>

namespace coresim {
class World;
class Renderer {
public:
    explicit Renderer(const std::filesystem::path& shader_directory);
    void draw(int framebuffer_width, int framebuffer_height, const glm::mat4& view_projection,
              const World& world);
private:
    Shader shader_;
    Mesh cube_;
    Mesh sphere_;
    int transform_location_{};
};
} // namespace coresim
