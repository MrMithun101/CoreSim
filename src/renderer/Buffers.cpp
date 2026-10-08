#include <coresim/renderer/Buffers.hpp>

#include <glad/gl.h>
#include <limits>
#include <stdexcept>
#include <utility>

namespace coresim {
namespace {
unsigned int upload(std::span<const std::byte> bytes) {
    if (bytes.empty() || bytes.size() > static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max())) {
        throw std::invalid_argument("GPU buffer size is empty or exceeds GLsizeiptr");
    }
    GLuint id = 0;
    glGenBuffers(1, &id);
    // Upload through ARRAY_BUFFER: ELEMENT_ARRAY_BUFFER would mutate the current VAO.
    glBindBuffer(GL_ARRAY_BUFFER, id);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(bytes.size()), bytes.data(), GL_STATIC_DRAW);
    const GLenum error = glGetError();
    if (id == 0 || error != GL_NO_ERROR) {
        glDeleteBuffers(1, &id);
        throw std::runtime_error("GPU buffer upload failed; OpenGL error " + std::to_string(error));
    }
    return id;
}
} // namespace

VertexBuffer::VertexBuffer(std::span<const float> data) : id_(upload(std::as_bytes(data))) {}
VertexBuffer::~VertexBuffer() { glDeleteBuffers(1, &id_); }
VertexBuffer::VertexBuffer(VertexBuffer&& other) noexcept : id_(std::exchange(other.id_, 0)) {}
VertexBuffer& VertexBuffer::operator=(VertexBuffer&& other) noexcept {
    if (this != &other) {
        glDeleteBuffers(1, &id_);
        id_ = std::exchange(other.id_, 0);
    }
    return *this;
}
void VertexBuffer::bind() const { glBindBuffer(GL_ARRAY_BUFFER, id_); }

IndexBuffer::IndexBuffer(std::span<const std::uint32_t> indices) {
    if (indices.size() > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
        throw std::invalid_argument("Index count exceeds GLsizei");
    }
    id_ = upload(std::as_bytes(indices));
    count_ = static_cast<int>(indices.size());
}
IndexBuffer::~IndexBuffer() { glDeleteBuffers(1, &id_); }
IndexBuffer::IndexBuffer(IndexBuffer&& other) noexcept
    : id_(std::exchange(other.id_, 0)), count_(std::exchange(other.count_, 0)) {}
IndexBuffer& IndexBuffer::operator=(IndexBuffer&& other) noexcept {
    if (this != &other) {
        glDeleteBuffers(1, &id_);
        id_ = std::exchange(other.id_, 0);
        count_ = std::exchange(other.count_, 0);
    }
    return *this;
}
void IndexBuffer::bind() const { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id_); }

VertexArray::VertexArray() {
    glGenVertexArrays(1, &id_);
    if (id_ == 0) {
        throw std::runtime_error("Could not allocate vertex array");
    }
}
VertexArray::~VertexArray() { glDeleteVertexArrays(1, &id_); }
VertexArray::VertexArray(VertexArray&& other) noexcept : id_(std::exchange(other.id_, 0)) {}
VertexArray& VertexArray::operator=(VertexArray&& other) noexcept {
    if (this != &other) {
        glDeleteVertexArrays(1, &id_);
        id_ = std::exchange(other.id_, 0);
    }
    return *this;
}
void VertexArray::bind() const { glBindVertexArray(id_); }
void VertexArray::configure_position_color(const VertexBuffer& vertices, const IndexBuffer& indices) {
    bind();
    vertices.bind();
    indices.bind();
    constexpr GLsizei stride = 6 * static_cast<GLsizei>(sizeof(float));
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}
} // namespace coresim
