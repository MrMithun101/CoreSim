#pragma once

#include <cstdint>
#include <span>

namespace coresim {

// All GPU owners require a current context for construction, use, and destruction.
class VertexBuffer {
public:
    explicit VertexBuffer(std::span<const float> data);
    ~VertexBuffer();
    VertexBuffer(const VertexBuffer&) = delete;
    VertexBuffer& operator=(const VertexBuffer&) = delete;
    VertexBuffer(VertexBuffer&& other) noexcept;
    VertexBuffer& operator=(VertexBuffer&& other) noexcept;
    void bind() const;
    [[nodiscard]] unsigned int id() const noexcept { return id_; }
private:
    unsigned int id_{};
};

class IndexBuffer {
public:
    explicit IndexBuffer(std::span<const std::uint32_t> indices);
    ~IndexBuffer();
    IndexBuffer(const IndexBuffer&) = delete;
    IndexBuffer& operator=(const IndexBuffer&) = delete;
    IndexBuffer(IndexBuffer&& other) noexcept;
    IndexBuffer& operator=(IndexBuffer&& other) noexcept;
    void bind() const;
    [[nodiscard]] int count() const noexcept { return count_; }
    [[nodiscard]] unsigned int id() const noexcept { return id_; }
private:
    unsigned int id_{};
    int count_{};
};

class VertexArray {
public:
    VertexArray();
    ~VertexArray();
    VertexArray(const VertexArray&) = delete;
    VertexArray& operator=(const VertexArray&) = delete;
    VertexArray(VertexArray&& other) noexcept;
    VertexArray& operator=(VertexArray&& other) noexcept;
    void bind() const;
    // Position (xyz) + color (rgb), six tightly packed floats per vertex.
    void configure_position_color(const VertexBuffer& vertices, const IndexBuffer& indices);
    [[nodiscard]] unsigned int id() const noexcept { return id_; }
private:
    unsigned int id_{};
};

} // namespace coresim
