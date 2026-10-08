#include <coresim/renderer/Renderer.hpp>

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <array>

namespace coresim {
namespace {
// Four vertices per face preserve flat face colors at shared spatial corners.
constexpr std::array<float, 144> cube_vertices{
    -1,-1, 1, .95F,.35F,.18F,  1,-1, 1, .95F,.35F,.18F,
     1, 1, 1, .95F,.35F,.18F, -1, 1, 1, .95F,.35F,.18F,
     1,-1,-1, .2F,.55F,.95F, -1,-1,-1, .2F,.55F,.95F,
    -1, 1,-1, .2F,.55F,.95F,  1, 1,-1, .2F,.55F,.95F,
    -1,-1,-1, .25F,.8F,.65F, -1,-1, 1, .25F,.8F,.65F,
    -1, 1, 1, .25F,.8F,.65F, -1, 1,-1, .25F,.8F,.65F,
     1,-1, 1, .8F,.4F,.9F,   1,-1,-1, .8F,.4F,.9F,
     1, 1,-1, .8F,.4F,.9F,   1, 1, 1, .8F,.4F,.9F,
    -1, 1, 1, 1.F,.8F,.25F,  1, 1, 1, 1.F,.8F,.25F,
     1, 1,-1, 1.F,.8F,.25F, -1, 1,-1, 1.F,.8F,.25F,
    -1,-1,-1, .3F,.4F,.65F,  1,-1,-1, .3F,.4F,.65F,
     1,-1, 1, .3F,.4F,.65F, -1,-1, 1, .3F,.4F,.65F,
};
constexpr std::array<std::uint32_t, 36> cube_indices{
    0,1,2, 2,3,0, 4,5,6, 6,7,4, 8,9,10, 10,11,8,
    12,13,14, 14,15,12, 16,17,18, 18,19,16, 20,21,22, 22,23,20
};
} // namespace

Renderer::Renderer(const std::filesystem::path& shader_directory)
    : shader_(shader_directory / "cube.vert", shader_directory / "cube.frag"),
      vertices_(cube_vertices), indices_(cube_indices) {
    vertex_array_.configure_position_color(vertices_, indices_);
    transform_location_ = shader_.uniform_location("u_transform");
}

void Renderer::draw(int framebuffer_width, int framebuffer_height,
                    const glm::mat4& view_projection, std::span<const Transform> transforms) {
    // Minimized windows may have zero-size framebuffers; avoid division by zero.
    if (framebuffer_width <= 0 || framebuffer_height <= 0) {
        return;
    }
    glViewport(0, 0, framebuffer_width, framebuffer_height);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glClearColor(0.035F, 0.055F, 0.085F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    shader_.bind();
    vertex_array_.bind();
    for (const auto& object : transforms) {
        const auto transform = view_projection * object.matrix();
        std::array<float, 16> matrix{};
        std::copy_n(glm::value_ptr(transform), matrix.size(), matrix.begin());
        shader_.set_matrix(transform_location_, matrix);
        glDrawElements(GL_TRIANGLES, indices_.count(), GL_UNSIGNED_INT, nullptr);
    }
    glBindVertexArray(0);
    glUseProgram(0);
}
} // namespace coresim
