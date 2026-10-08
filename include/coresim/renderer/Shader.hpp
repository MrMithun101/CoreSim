#pragma once

#include <array>
#include <filesystem>

namespace coresim {
class Shader {
public:
    Shader(const std::filesystem::path& vertex_path, const std::filesystem::path& fragment_path);
    ~Shader();
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;
    void bind() const;
    [[nodiscard]] int uniform_location(const char* name) const;
    // The program must be bound. Column-major matrix, matching GLSL.
    void set_matrix(int location, const std::array<float, 16>& matrix) const;
    [[nodiscard]] unsigned int id() const noexcept { return id_; }
private:
    unsigned int id_{};
};
} // namespace coresim
