#pragma once

#include <coresim/renderer/Mesh.hpp>
#include <coresim/renderer/Shader.hpp>
#include <filesystem>
#include <glm/mat4x4.hpp>
#include <vector>
#include <array>

namespace coresim {
class World;
enum class RenderMode { individual, instanced };
struct RenderStats {
    std::size_t objects{}, draw_calls{}, uploaded_bytes{};
};
class Renderer {
public:
    explicit Renderer(const std::filesystem::path& shader_directory);
    void draw(int framebuffer_width, int framebuffer_height, const glm::mat4& view_projection,
              const World& world, RenderMode mode = RenderMode::instanced);
    [[nodiscard]] const RenderStats& stats() const noexcept { return stats_; }
private:
    Shader shader_;
    Shader instanced_shader_;
    std::array<std::vector<std::array<float, 16>>, 2> batches_;
    RenderStats stats_;
    Mesh cube_;
    Mesh sphere_;
    int transform_location_{};
};
} // namespace coresim
