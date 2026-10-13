#include <coresim/renderer/Mesh.hpp>
#include <glad/gl.h>
#include <limits>
#include <stdexcept>

namespace coresim {
Mesh::Mesh(std::span<const float> vertices, std::span<const std::uint32_t> indices)
    : vertices_(vertices), indices_(indices) {
    array_.configure_position_color(vertices_, indices_);
    array_.bind();
    instances_.bind();
    for (GLuint column = 0; column < 4; ++column) {
        glVertexAttribPointer(2 + column, 4, GL_FLOAT, GL_FALSE, 16 * sizeof(float),
                              reinterpret_cast<const void*>(static_cast<std::uintptr_t>(column * 4 * sizeof(float))));
        glEnableVertexAttribArray(2 + column);
        glVertexAttribDivisor(2 + column, 1);
    }
    glBindVertexArray(0);
}
void Mesh::draw_instanced(std::span<const std::array<float, 16>> transforms) {
    if (transforms.empty()) { return; }
    if (transforms.size() > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
        throw std::invalid_argument("Instance count exceeds GLsizei");
    }
    instances_.stream(std::as_bytes(transforms));
    array_.bind();
    glDrawElementsInstanced(GL_TRIANGLES, indices_.count(), GL_UNSIGNED_INT, nullptr,
                           static_cast<GLsizei>(transforms.size()));
}
void Mesh::draw() const {
    array_.bind();
    glDrawElements(GL_TRIANGLES, indices_.count(), GL_UNSIGNED_INT, nullptr);
}
} // namespace coresim
