#include <coresim/core/Window.hpp>
#include <coresim/renderer/Renderer.hpp>
#include <coresim/scene/World.hpp>
#include <glad/gl.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <set>
#include <string>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
double milliseconds(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b-a).count();
}
struct Target {
    GLuint framebuffer{}, texture{}, depth{};
    static constexpr int width=1280, height=720;
    Target() {
        glGenFramebuffers(1,&framebuffer); glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
        glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
        glGenRenderbuffers(1,&depth); glBindRenderbuffer(GL_RENDERBUFFER,depth);
        glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,width,height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE || glGetError()!=GL_NO_ERROR) {
            release(); throw std::runtime_error("Benchmark framebuffer allocation failed");
        }
    }
    ~Target() { release(); }
    Target(const Target&)=delete;
    Target& operator=(const Target&)=delete;
    void release() {
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        glDeleteRenderbuffers(1,&depth); glDeleteTextures(1,&texture); glDeleteFramebuffers(1,&framebuffer);
    }
    std::uint64_t checksum() const {
        std::vector<unsigned char> pixels(static_cast<std::size_t>(width)*height*4);
        glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        std::uint64_t hash=14695981039346656037ULL;
        for (auto p:pixels) { hash^=p; hash*=1099511628211ULL; }
        return hash;
    }
};
std::size_t number(const std::string& value, std::size_t maximum, bool zero=false) {
    std::size_t n{};
    const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),n);
    if (error!=std::errc{} || end!=value.data()+value.size() || (!zero && n==0) || n>maximum) {
        throw std::invalid_argument("Invalid numeric option: "+value);
    }
    return n;
}
}
int main(int argc,char** argv) {
    try {
        std::size_t objects=1000, frames=30, warmup=10;
        std::string mode="individual", geometry="cubes";
        std::set<std::string> seen;
        for (int i=1;i<argc;++i) {
            const std::string key=argv[i];
            if (key=="--help") {
                std::cout<<"coresim_render_benchmark --mode individual|instanced --objects 1..100000 --frames 1..10000 --warmup 0..10000 --geometry cubes|mixed\n";
                return 0;
            }
            if (!seen.insert(key).second || i+1>=argc) { throw std::invalid_argument("Duplicate or missing option: "+key); }
            const std::string value=argv[++i];
            if (key=="--objects") { objects=number(value,100000); }
            else if (key=="--frames") { frames=number(value,10000); }
            else if (key=="--warmup") { warmup=number(value,10000,true); }
            else if (key=="--mode") { mode=value; }
            else if (key=="--geometry") { geometry=value; }
            else { throw std::invalid_argument("Unknown option: "+key); }
        }
        if ((mode!="individual" && mode!="instanced") || (geometry!="cubes" && geometry!="mixed")) { throw std::invalid_argument("Invalid rendering mode or geometry"); }
        coresim::GlfwRuntime runtime;
        coresim::Window window(runtime,Target::width,Target::height,false);
        coresim::Renderer renderer(CORESIM_SHADER_DIR);
        Target target;
        coresim::World world;
        const auto columns=static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<double>(objects)*16.0/9.0)));
        const auto rows=(objects+columns-1)/columns;
        for (std::size_t i=0;i<objects;++i) {
            const auto e=world.create();
            coresim::Transform t;
            t.position={3*static_cast<float>(i%columns),3*static_cast<float>(i/columns),0};
            t.scale=glm::vec3(0.8F);
            t.rotation=glm::angleAxis(0.35F,glm::normalize(glm::vec3(1,2,3)));
            world.set_transform(e,t);
            world.set_mesh(e,{geometry=="mixed" && i%2 ? coresim::MeshKind::sphere : coresim::MeshKind::cube});
        }
        const auto projection=glm::ortho(-2.F,3*static_cast<float>(columns),-2.F,3*static_cast<float>(rows),-5.F,5.F);
        std::cerr<<"build="<<CORESIM_BUILD_TYPE<<" compiler="<<CORESIM_COMPILER
                 <<"\nGL_VENDOR="<<glGetString(GL_VENDOR)<<"\nGL_RENDERER="<<glGetString(GL_RENDERER)
                 <<"\nGL_VERSION="<<glGetString(GL_VERSION)<<"\n";
        struct Sample { double submission,frame; };
        std::vector<Sample> samples; samples.reserve(frames);
        glFinish();
        for (std::size_t i=0;i<warmup+frames;++i) {
            window.process_events();
            const auto start=Clock::now();
            renderer.draw(Target::width,Target::height,projection,world,mode=="individual" ? coresim::RenderMode::individual : coresim::RenderMode::instanced);
            const auto submitted=Clock::now();
            glFinish(); // Bound each sample to completed work; no queued frames or swap/VSync.
            const auto completed=Clock::now();
            if (glGetError()!=GL_NO_ERROR) { throw std::runtime_error("Rendering benchmark OpenGL error"); }
            if (i>=warmup) { samples.push_back({milliseconds(start,submitted),milliseconds(start,completed)}); }
        }
        const auto checksum=target.checksum();
        if (glGetError()!=GL_NO_ERROR) { throw std::runtime_error("Framebuffer readback failed"); }
        std::cout<<"schema,mode,geometry,objects,draw_calls,width,height,sample,cpu_submission_ms,frame_ms,fps,image_checksum\n"<<std::setprecision(12);
        for (std::size_t i=0;i<samples.size();++i) {
            const auto s=samples[i];
            std::cout<<"1,"<<mode<<','<<geometry<<','<<objects<<','<<renderer.stats().draw_calls<<','<<Target::width<<','<<Target::height<<','<<i<<','<<s.submission<<','<<s.frame<<','<<1000/s.frame<<','<<checksum<<'\n';
        }
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
