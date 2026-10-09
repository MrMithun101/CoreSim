#pragma once
#include <coresim/renderer/Buffers.hpp>

namespace coresim {
class Mesh {
public:
    Mesh(std::span<const float> vertices, std::span<const std::uint32_t> indices);
    void draw() const;
private:
    VertexBuffer vertices_;
    IndexBuffer indices_;
    VertexArray array_;
};
} // namespace coresim
