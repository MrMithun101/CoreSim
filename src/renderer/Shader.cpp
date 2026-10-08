#include <coresim/renderer/Shader.hpp>

#include <glad/gl.h>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace coresim {
namespace {
std::string read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open shader file: " + path.string());
    }
    std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    if (file.bad() || text.empty()) {
        throw std::runtime_error("Cannot read shader or file is empty: " + path.string());
    }
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<GLint>::max())) {
        throw std::runtime_error("Shader file is too large: " + path.string());
    }
    return text;
}

struct Stage {
    GLuint id{};
    Stage(GLenum type, const std::filesystem::path& path) {
        const auto source = read_file(path);
        id = glCreateShader(type);
        if (id == 0) {
            throw std::runtime_error("Cannot create shader: " + path.string());
        }
        try {
            const char* data = source.data();
            const auto length = static_cast<GLint>(source.size());
            glShaderSource(id, 1, &data, &length);
            glCompileShader(id);
            GLint success = GL_FALSE;
            glGetShaderiv(id, GL_COMPILE_STATUS, &success);
            if (success != GL_TRUE) {
                GLint size = 0;
                glGetShaderiv(id, GL_INFO_LOG_LENGTH, &size);
                std::string log(static_cast<std::size_t>(size > 0 ? size : 1), '\0');
                glGetShaderInfoLog(id, static_cast<GLsizei>(log.size()), nullptr, log.data());
                throw std::runtime_error("Shader compilation failed (" + path.string() + "):\n" + log);
            }
        } catch (...) {
            glDeleteShader(id);
            throw;
        }
    }
    ~Stage() { glDeleteShader(id); }
    Stage(const Stage&) = delete;
    Stage& operator=(const Stage&) = delete;
};
} // namespace

Shader::Shader(const std::filesystem::path& vertex_path, const std::filesystem::path& fragment_path) {
    const Stage vertex(GL_VERTEX_SHADER, vertex_path);
    const Stage fragment(GL_FRAGMENT_SHADER, fragment_path);
    id_ = glCreateProgram();
    if (id_ == 0) {
        throw std::runtime_error("Cannot create shader program");
    }
    try {
        glAttachShader(id_, vertex.id);
        glAttachShader(id_, fragment.id);
        glLinkProgram(id_);
        GLint success = GL_FALSE;
        glGetProgramiv(id_, GL_LINK_STATUS, &success);
        if (success != GL_TRUE) {
            GLint size = 0;
            glGetProgramiv(id_, GL_INFO_LOG_LENGTH, &size);
            std::string log(static_cast<std::size_t>(size > 0 ? size : 1), '\0');
            glGetProgramInfoLog(id_, static_cast<GLsizei>(log.size()), nullptr, log.data());
            throw std::runtime_error("Shader link failed (" + vertex_path.string() + ", " +
                                     fragment_path.string() + "):\n" + log);
        }
        glDetachShader(id_, vertex.id);
        glDetachShader(id_, fragment.id);
    } catch (...) {
        glDeleteProgram(id_);
        throw;
    }
}
Shader::~Shader() { glDeleteProgram(id_); }
Shader::Shader(Shader&& other) noexcept : id_(std::exchange(other.id_, 0)) {}
Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        glDeleteProgram(id_);
        id_ = std::exchange(other.id_, 0);
    }
    return *this;
}
void Shader::bind() const { glUseProgram(id_); }
int Shader::uniform_location(const char* name) const {
    const GLint location = glGetUniformLocation(id_, name);
    if (location < 0) {
        throw std::runtime_error(std::string("Missing active shader uniform: ") + name);
    }
    return location;
}
void Shader::set_matrix(int location, const std::array<float, 16>& matrix) const {
    glUniformMatrix4fv(location, 1, GL_FALSE, matrix.data());
}
} // namespace coresim
