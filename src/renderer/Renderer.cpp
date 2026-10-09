#include <coresim/renderer/Renderer.hpp>
#include <coresim/scene/World.hpp>

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <array>
#include <vector>
#include <cmath>
#include <numbers>

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
Mesh make_sphere() {
    constexpr std::uint32_t rings = 16, sectors = 24;
    std::vector<float> vertices;
    std::vector<std::uint32_t> indices;
    vertices.reserve((rings + 1) * (sectors + 1) * 6);
    indices.reserve(rings * sectors * 6);
    for (std::uint32_t ring = 0; ring <= rings; ++ring) {
        const float latitude = std::numbers::pi_v<float> * static_cast<float>(ring) / rings;
        for (std::uint32_t sector = 0; sector <= sectors; ++sector) {
            const float longitude = 2 * std::numbers::pi_v<float> * static_cast<float>(sector) / sectors;
            const float x = std::sin(latitude) * std::cos(longitude);
            const float y = std::cos(latitude);
            const float z = std::sin(latitude) * std::sin(longitude);
            vertices.insert(vertices.end(), {x, y, z, 0.35F + 0.3F * x, 0.55F + 0.3F * y, 0.7F + 0.25F * z});
        }
    }
    for (std::uint32_t ring = 0; ring < rings; ++ring) {
        for (std::uint32_t sector = 0; sector < sectors; ++sector) {
            const auto a = ring * (sectors + 1) + sector;
            const auto b = a + sectors + 1;
            indices.insert(indices.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
    }
    return Mesh(vertices, indices);
}
} // namespace

Renderer::Renderer(const std::filesystem::path& shader_directory)
    : shader_(shader_directory / "cube.vert", shader_directory / "cube.frag"),
      cube_(cube_vertices, cube_indices), sphere_(make_sphere()) {
    transform_location_ = shader_.uniform_location("u_transform");
}

void Renderer::draw(int framebuffer_width, int framebuffer_height,
                    const glm::mat4& view_projection, const World& world) {
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
    for (const auto& entry : world.meshes()) {
        const auto* object = world.transform(entry.entity);
        if (!object) {
            continue;
        }
        const auto transform = view_projection * object->matrix();
        std::array<float, 16> matrix{};
        std::copy_n(glm::value_ptr(transform), matrix.size(), matrix.begin());
        shader_.set_matrix(transform_location_, matrix);
        switch (entry.value.kind) {
        case MeshKind::cube: cube_.draw(); break;
        case MeshKind::sphere: sphere_.draw(); break;
        }
    }
    glBindVertexArray(0);
    glUseProgram(0);
}
} // namespace coresim
