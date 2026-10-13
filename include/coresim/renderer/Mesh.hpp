#pragma once
#include <coresim/renderer/Buffers.hpp>
#include <array>

namespace coresim {
class Mesh {
public:
    Mesh(std::span<const float> vertices, std::span<const std::uint32_t> indices);
    void draw() const;
    void draw_instanced(std::span<const std::array<float, 16>> transforms);
private:
    VertexBuffer vertices_;
    IndexBuffer indices_;
    VertexBuffer instances_;
    VertexArray array_;
};
} // namespace coresim
