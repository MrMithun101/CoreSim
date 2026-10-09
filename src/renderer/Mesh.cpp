#include <coresim/renderer/Mesh.hpp>
#include <glad/gl.h>

namespace coresim {
Mesh::Mesh(std::span<const float> vertices, std::span<const std::uint32_t> indices)
    : vertices_(vertices), indices_(indices) {
    array_.configure_position_color(vertices_, indices_);
}
void Mesh::draw() const {
    array_.bind();
    glDrawElements(GL_TRIANGLES, indices_.count(), GL_UNSIGNED_INT, nullptr);
}
} // namespace coresim
