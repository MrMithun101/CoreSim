#pragma once

#include <coresim/renderer/Buffers.hpp>
#include <coresim/renderer/Shader.hpp>
#include <filesystem>

namespace coresim {
class Renderer {
public:
    explicit Renderer(const std::filesystem::path& shader_directory);
    void draw(int framebuffer_width, int framebuffer_height, double angle_radians);
private:
    Shader shader_;
    VertexBuffer vertices_;
    IndexBuffer indices_;
    VertexArray vertex_array_;
    int transform_location_{};
};
} // namespace coresim
