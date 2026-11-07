#include <coresim/renderer/Renderer.hpp>
#include <coresim/scene/World.hpp>

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <array>
#include <vector>
#include <cmath>
#include <numbers>
#include <glm/gtc/matrix_transform.hpp>

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

struct Renderer::Shadows {
    static constexpr int resolution=2048;
    Shader program;
    GLuint fbo{},texture{};
    int light;
    explicit Shadows(const std::filesystem::path& path):program(path/"shadow.vert",path/"shadow.frag"),light(program.uniform_location("u_light")){
        GLint old_draw{},old_read{};glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&old_draw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&old_read);
        glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
        glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,resolution,resolution,0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,texture,0);
        glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);
        bool ok=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,static_cast<GLuint>(old_draw));glBindFramebuffer(GL_READ_FRAMEBUFFER,static_cast<GLuint>(old_read));
        glBindTexture(GL_TEXTURE_2D,0);
        if(!ok){glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&texture);throw std::runtime_error("Shadow framebuffer incomplete");}
    }
    ~Shadows(){glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&texture);}
};
Renderer::~Renderer()=default;
Renderer::Renderer(const std::filesystem::path& shader_directory)
    : shaders_(shader_directory),shader_(shader_directory / "cube.vert", shader_directory / "cube.frag"),
      instanced_shader_(shader_directory / "instanced.vert", shader_directory / "cube.frag"),
      cube_(cube_vertices, cube_indices), sphere_(make_sphere()) {
    auto locations=[](const Shader& s){return Lighting{s.uniform_location("u_vp"),s.uniform_location("u_light"),s.uniform_location("u_eye"),s.uniform_location("u_round"),s.uniform_location("u_depth")};};
    individual_light_=locations(shader_);instance_light_=locations(instanced_shader_);
    finish_location_=shader_.uniform_location("u_finish");
    instance_finish_=instanced_shader_.uniform_location("u_finish");
    transform_location_ = shader_.uniform_location("u_transform");
}

void Renderer::draw(int framebuffer_width, int framebuffer_height,
                    const glm::mat4& view_projection, const World& world, RenderMode mode,glm::vec3 eye) {
    stats_ = {};
    for (auto& batch : batches_) { batch.clear(); }
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
    bool facility=false;
    for(const auto& entry:world.meshes()){
        const auto* object=world.transform(entry.entity);if(!object)continue;
        ++stats_.objects;
        const bool lit=entry.value.finish!=Finish::legacy;facility|=lit;
        if(mode==RenderMode::individual && !lit)continue;
        auto transform=object->matrix();if(!lit)transform=view_projection*transform;
        std::array<float,16> matrix;std::copy_n(glm::value_ptr(transform),16,matrix.begin());
        batches_.at(2*static_cast<std::size_t>(entry.value.finish)+(entry.value.kind==MeshKind::sphere?1U:0U)).push_back(matrix);
    }
    glm::mat4 light(1);
    if(facility){
        if(!shadows_)shadows_=std::make_unique<Shadows>(shaders_);
        light=glm::ortho(-23.F,23.F,-23.F,23.F,1.F,100.F)*glm::lookAt(glm::vec3(-16,32,20),glm::vec3(0),glm::vec3(0,1,0));
        GLint framebuffer{};glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&framebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,shadows_->fbo);glViewport(0,0,Shadows::resolution,Shadows::resolution);
        glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(2.F,4.F);
        glClear(GL_DEPTH_BUFFER_BIT);shadows_->program.bind();glUniformMatrix4fv(shadows_->light,1,GL_FALSE,glm::value_ptr(light));
        for(std::size_t i=2;i<batches_.size();++i)if(!batches_[i].empty()){
            (i%2?sphere_:cube_).draw_instanced(batches_[i]);++stats_.draw_calls;stats_.uploaded_bytes+=batches_[i].size()*sizeof(batches_[i][0]);
        }
        glDisable(GL_POLYGON_OFFSET_FILL);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,static_cast<GLuint>(framebuffer));glViewport(0,0,framebuffer_width,framebuffer_height);
    }
    const bool single=mode==RenderMode::individual;
    (single?shader_:instanced_shader_).bind();
    auto loc=single?individual_light_:instance_light_;
    auto finish=single?finish_location_:instance_finish_;
    if(facility){
        glUniformMatrix4fv(loc.vp,1,GL_FALSE,glm::value_ptr(view_projection));glUniformMatrix4fv(loc.light,1,GL_FALSE,glm::value_ptr(light));
        glUniform3fv(loc.eye,1,glm::value_ptr(eye));glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,shadows_->texture);glUniform1i(loc.depth,0);
    }
    if(single){
        for(const auto& entry:world.meshes()){
            const auto* object=world.transform(entry.entity);if(!object)continue;
            auto transform=object->matrix();if(entry.value.finish==Finish::legacy)transform=view_projection*transform;
            glUniformMatrix4fv(transform_location_,1,GL_FALSE,glm::value_ptr(transform));glUniform1i(finish,static_cast<int>(entry.value.finish));
            glUniform1i(loc.round,entry.value.kind==MeshKind::sphere);(entry.value.kind==MeshKind::sphere?sphere_:cube_).draw();++stats_.draw_calls;
        }
    }else{
        for(std::size_t i=0;i<batches_.size();++i){
            auto& batch=batches_[i];if(batch.empty())continue;
            glUniform1i(finish,static_cast<int>(i/2));glUniform1i(loc.round,static_cast<int>(i%2));
            (i%2?sphere_:cube_).draw_instanced(batch);++stats_.draw_calls;stats_.uploaded_bytes+=batch.size()*sizeof(batch[0]);
        }
    }
    if(facility)glBindTexture(GL_TEXTURE_2D,0);
    glBindVertexArray(0);
    glUseProgram(0);
}
} // namespace coresim
