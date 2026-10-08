#pragma once

#include <coresim/renderer/Buffers.hpp>
#include <coresim/renderer/Shader.hpp>
#include <filesystem>
#include <coresim/scene/Transform.hpp>
#include <span>

namespace coresim {
class Renderer {
public:
    explicit Renderer(const std::filesystem::path& shader_directory);
    void draw(int framebuffer_width, int framebuffer_height, const glm::mat4& view_projection,
              std::span<const Transform> transforms);
private:
    Shader shader_;
    VertexBuffer vertices_;
    IndexBuffer indices_;
    VertexArray vertex_array_;
    int transform_location_{};
};
} // namespace coresim
