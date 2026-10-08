#include <coresim/core/Window.hpp>
#include <coresim/renderer/Renderer.hpp>
#include <coresim/scene/Camera.hpp>
#include <coresim/scene/DemoScene.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include <glad/gl.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <array>
#include <cstdlib>
#include <fstream>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
using namespace coresim;
using Catch::Matchers::ContainsSubstring;
const std::filesystem::path shaders = CORESIM_SHADER_DIR;
const std::filesystem::path fixtures = CORESIM_TEST_SHADER_DIR;
constexpr std::array<float, 16> identity{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
constexpr std::array<float, 18> triangle{-1,-1,0, 1,0,0, 1,-1,0, 1,0,0, 0,1,0, 1,0,0};
constexpr std::array<std::uint32_t, 3> indices{0,1,2};

struct Context {
    GlfwRuntime runtime;
    Window window{runtime, 256, 256, false};
};

// An explicit framebuffer avoids hidden-window backing-store behavior on different OSes.
struct Target {
    static constexpr int size = 256;
    GLuint framebuffer{}, texture{}, depth{};
    Target() {
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        glGenRenderbuffers(1, &depth);
        glBindRenderbuffer(GL_RENDERBUFFER, depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size, size);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            release();
            throw std::runtime_error("Test framebuffer is incomplete");
        }
    }
    ~Target() { release(); }
    Target(const Target&) = delete;
    Target& operator=(const Target&) = delete;
    void release() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteRenderbuffers(1, &depth);
        glDeleteTextures(1, &texture);
        glDeleteFramebuffers(1, &framebuffer);
    }
    std::array<unsigned char, 4> center() const {
        std::array<unsigned char, 4> pixel{};
        glReadPixels(size / 2, size / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        return pixel;
    }
    std::vector<unsigned char> pixels() const {
        std::vector<unsigned char> result(size * size * 4);
        glReadPixels(0, 0, size, size, GL_RGBA, GL_UNSIGNED_BYTE, result.data());
        return result;
    }
};

void draw_single(Renderer& renderer, int width, int height, float angle) {
    Transform transform;
    transform.rotation = glm::angleAxis(angle, glm::normalize(glm::vec3(0.35F, 1.0F, 0.2F)));
    const auto view = glm::lookAt(glm::vec3(3.5F, 2.5F, 5.0F), glm::vec3(0), glm::vec3(0, 1, 0));
    const float aspect = height > 0 && width > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0F;
    const auto projection = glm::perspective(glm::radians(45.0F), aspect, 0.1F, 100.0F);
    World world;
    const auto entity = world.create();
    world.set_transform(entity, transform);
    world.set_mesh(entity);
    renderer.draw(width, height, projection * view, world);
}

static_assert(!std::is_copy_constructible_v<Shader>);
static_assert(!std::is_copy_constructible_v<VertexBuffer>);
static_assert(!std::is_copy_constructible_v<IndexBuffer>);
static_assert(!std::is_copy_constructible_v<VertexArray>);
static_assert(std::is_nothrow_move_constructible_v<Shader>);
static_assert(std::is_nothrow_move_assignable_v<VertexBuffer>);

TEST_CASE("GPU owners transfer handles and delete replaced resources") {
    Context context;
    SECTION("vertex buffer") {
        GLuint surviving = 0;
        {
            VertexBuffer source(triangle);
            surviving = source.id();
            VertexBuffer moved(std::move(source));
            REQUIRE(source.id() == 0);
            REQUIRE(moved.id() == surviving);
            VertexBuffer destination(triangle);
            const auto replaced = destination.id();
            destination = std::move(moved);
            REQUIRE(moved.id() == 0);
            REQUIRE(glIsBuffer(replaced) == GL_FALSE);
            REQUIRE(glIsBuffer(surviving) == GL_TRUE);
        }
        REQUIRE(glIsBuffer(surviving) == GL_FALSE);
    }
    SECTION("index buffer") {
        IndexBuffer source(indices);
        const auto original = source.id();
        IndexBuffer moved(std::move(source));
        REQUIRE(source.count() == 0);
        IndexBuffer destination(indices);
        const auto replaced = destination.id();
        destination = std::move(moved);
        REQUIRE(moved.id() == 0);
        REQUIRE(destination.id() == original);
        REQUIRE(destination.count() == 3);
        REQUIRE(glIsBuffer(replaced) == GL_FALSE);
    }
    SECTION("vertex array") {
        VertexArray source;
        source.bind();
        const auto original = source.id();
        VertexArray moved(std::move(source));
        REQUIRE(source.id() == 0);
        VertexArray destination;
        destination.bind();
        const auto replaced = destination.id();
        destination = std::move(moved);
        REQUIRE(moved.id() == 0);
        REQUIRE(destination.id() == original);
        REQUIRE(glIsVertexArray(replaced) == GL_FALSE);
        destination.bind();
        REQUIRE(glIsVertexArray(original) == GL_TRUE);
    }
    SECTION("shader") {
        GLuint surviving = 0;
        {
            Shader source(shaders / "cube.vert", shaders / "cube.frag");
            surviving = source.id();
            Shader moved(std::move(source));
            REQUIRE(source.id() == 0);
            Shader destination(shaders / "cube.vert", shaders / "cube.frag");
            const auto replaced = destination.id();
            destination = std::move(moved);
            REQUIRE(moved.id() == 0);
            REQUIRE(destination.id() == surviving);
            REQUIRE(glIsProgram(replaced) == GL_FALSE);
            REQUIRE(glIsProgram(surviving) == GL_TRUE);
        }
        REQUIRE(glIsProgram(surviving) == GL_FALSE);
    }
    REQUIRE(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Shader failures report file names and compilation or link stage") {
    Context context;
    REQUIRE_THROWS_WITH(Shader(shaders / "missing.vert", shaders / "cube.frag"),
                        ContainsSubstring("missing.vert"));
    REQUIRE_THROWS_WITH(Shader(shaders / "cube.vert", fixtures / "invalid.frag"),
                        ContainsSubstring("Shader compilation failed") && ContainsSubstring("invalid.frag"));
    REQUIRE_THROWS_WITH(Shader(shaders / "cube.vert", fixtures / "mismatch.frag"),
                        ContainsSubstring("Shader link failed") && ContainsSubstring("mismatch.frag"));
    Shader valid(shaders / "cube.vert", shaders / "cube.frag");
    REQUIRE_THROWS_WITH(valid.uniform_location("missing_uniform"), ContainsSubstring("missing_uniform"));
    REQUIRE(valid.uniform_location("u_transform") >= 0);
    REQUIRE(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Renderer produces a cube, rotates it, and handles an empty framebuffer") {
    Context context;
    Target target;
    Renderer renderer(shaders);
    draw_single(renderer, Target::size, Target::size, 0.0F);
    REQUIRE(glIsEnabled(GL_DEPTH_TEST) == GL_TRUE);
    float depth = 1.0F;
    glReadPixels(Target::size / 2, Target::size / 2, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
    REQUIRE(depth < 1.0F);
    const auto first = target.pixels();
    const auto center = target.center();
    REQUIRE((center[0] > 50 || center[1] > 50 || center[2] > 50));
    draw_single(renderer, Target::size, Target::size, 1.0F);
    REQUIRE(first != target.pixels());
    draw_single(renderer, 0, 0, 0.0F);
    draw_single(renderer, 128, 256, 0.5F);
    std::array<GLint, 4> viewport{};
    glGetIntegerv(GL_VIEWPORT, viewport.data());
    REQUIRE(viewport[2] == 128);
    REQUIRE(viewport[3] == 256);
    REQUIRE(glGetError() == GL_NO_ERROR);
    if (const char* path = std::getenv("CORESIM_TEST_IMAGE")) {
        std::ofstream image(path, std::ios::binary);
        image << "P6\n" << Target::size << ' ' << Target::size << "\n255\n";
        for (int y = Target::size - 1; y >= 0; --y) {
            for (int x = 0; x < Target::size; ++x) {
                const auto offset = static_cast<std::size_t>((y * Target::size + x) * 4);
                image.write(reinterpret_cast<const char*>(first.data() + offset), 3);
            }
        }
        REQUIRE(image.good());
    }
}

TEST_CASE("Depth testing keeps the nearer surface even when the farther one is drawn last") {
    Context context;
    Target target;
    Renderer renderer(shaders);
    draw_single(renderer, Target::size, Target::size, 0.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // Red triangle at z=-0.5, blue triangle at z=+0.5, identical coverage.
    constexpr std::array<float, 36> vertices{
        -1,-1,-.5F, 1,0,0, 1,-1,-.5F, 1,0,0, 0,1,-.5F, 1,0,0,
        -1,-1, .5F, 0,0,1, 1,-1, .5F, 0,0,1, 0,1, .5F, 0,0,1};
    constexpr std::array<std::uint32_t, 6> order{0,1,2,3,4,5};
    VertexBuffer buffer(vertices);
    IndexBuffer elements(order);
    VertexArray array;
    array.configure_position_color(buffer, elements);
    Shader shader(shaders / "cube.vert", shaders / "cube.frag");
    shader.bind();
    shader.set_matrix(shader.uniform_location("u_transform"), identity);
    array.bind();
    glDrawElements(GL_TRIANGLES, elements.count(), GL_UNSIGNED_INT, nullptr);
    REQUIRE(target.center()[0] > 240);
    REQUIRE(target.center()[2] < 10);
    // Control: the identical draw without depth testing must produce blue.
    glDisable(GL_DEPTH_TEST);
    glDrawElements(GL_TRIANGLES, elements.count(), GL_UNSIGNED_INT, nullptr);
    REQUIRE(target.center()[0] < 10);
    REQUIRE(target.center()[2] > 240);
    glUseProgram(0);
    glBindVertexArray(0);
    REQUIRE(glGetError() == GL_NO_ERROR);
}
} // namespace

TEST_CASE("Renderer draws independent transforms and responds to camera motion") {
    Context context;
    Target target;
    Renderer renderer(shaders);
    World objects;
    const auto left_entity = objects.create();
    const auto right_entity = objects.create();
    objects.set_transform(left_entity).position = {-0.5F, 0, 0};
    objects.set_transform(right_entity).position = {0.5F, 0, 0};
    objects.transform(left_entity)->scale = objects.transform(right_entity)->scale = glm::vec3(0.2F);
    objects.set_mesh(left_entity);
    objects.set_mesh(right_entity);
    renderer.draw(Target::size, Target::size, glm::mat4(1), objects);
    const auto before = target.pixels();
    const auto left = static_cast<std::size_t>((Target::size / 2 * Target::size + Target::size / 4) * 4);
    const auto right = static_cast<std::size_t>((Target::size / 2 * Target::size + 3 * Target::size / 4) * 4);
    REQUIRE(before[left + 2] > 100);
    REQUIRE(before[right + 2] > 100);
    objects.transform(right_entity)->position.y = 0.7F;
    renderer.draw(Target::size, Target::size, glm::mat4(1), objects);
    const auto after = target.pixels();
    REQUIRE(after[left + 2] == before[left + 2]);
    REQUIRE(after[right + 2] < 50);

    DemoScene scene;
    Camera camera({16, 18, 27}, -2.106F, -0.52F);
    renderer.draw(Target::size, Target::size, camera.projection(1) * camera.view(), scene.world());
    const auto initial = target.pixels();
    camera.move({1, 0, 1}, 0.5F);
    camera.look(0.1F, 0.05F);
    renderer.draw(Target::size, Target::size, camera.projection(1) * camera.view(), scene.world());
    REQUIRE(initial != target.pixels());
    REQUIRE(glGetError() == GL_NO_ERROR);
    if (const char* path = std::getenv("CORESIM_SCENE_IMAGE")) {
        std::ofstream image(path, std::ios::binary);
        image << "P6\n" << Target::size << ' ' << Target::size << "\n255\n";
        for (int y = Target::size - 1; y >= 0; --y) {
            for (int x = 0; x < Target::size; ++x) {
                const auto offset = static_cast<std::size_t>((y * Target::size + x) * 4);
                image.write(reinterpret_cast<const char*>(initial.data() + offset), 3);
            }
        }
        REQUIRE(image.good());
    }
}

TEST_CASE("Rendering joins mesh and transform components and excludes destroyed entities") {
    Context context;
    Target target;
    Renderer renderer(shaders);
    World world;
    const auto entity = world.create();
    world.set_transform(entity).scale = glm::vec3(0.4F);
    renderer.draw(Target::size, Target::size, glm::mat4(1), world);
    const auto background = target.pixels();
    world.set_mesh(entity);
    renderer.draw(Target::size, Target::size, glm::mat4(1), world);
    const auto visible = target.pixels();
    REQUIRE(visible != background);
    REQUIRE(world.remove_transform(entity));
    renderer.draw(Target::size, Target::size, glm::mat4(1), world);
    REQUIRE(target.pixels() == background);
    world.set_transform(entity).scale = glm::vec3(0.4F);
    renderer.draw(Target::size, Target::size, glm::mat4(1), world);
    REQUIRE(target.pixels() == visible);
    REQUIRE(world.destroy(entity));
    const auto replacement = world.create();
    REQUIRE(replacement.index == entity.index);
    REQUIRE(replacement.generation != entity.generation);
    renderer.draw(Target::size, Target::size, glm::mat4(1), world);
    REQUIRE(target.pixels() == background);
    REQUIRE(glGetError() == GL_NO_ERROR);
}
