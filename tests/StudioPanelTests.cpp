#include <coresim/core/Window.hpp>
#include <coresim/ui/ProfilerPanel.hpp>
#include <coresim/ui/StudioPanel.hpp>
#include <coresim/studio/StudioSession.hpp>
#include <coresim/renderer/Renderer.hpp>
#include <imgui.h>
#include <glad/gl.h>
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <fstream>
#include <vector>
using namespace coresim;
TEST_CASE("Studio controls select scenes and drive paused simulation through the UI") {
    GlfwRuntime runtime;
    Window window(runtime,1280,720,false);
    ProfilerPanel ui(window);
    StudioPanel panel;
    StudioSession session;
    Renderer renderer(CORESIM_SHADER_DIR);
    auto camera=session.scene().camera();
    Profiler profiler;
    GLuint framebuffer{},texture{},depth{};
    glGenFramebuffers(1,&framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
    glGenTextures(1,&texture);
    glBindTexture(GL_TEXTURE_2D,texture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1280,720,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
    glGenRenderbuffers(1,&depth);
    glBindRenderbuffer(GL_RENDERBUFFER,depth);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,1280,720);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
    REQUIRE(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    const auto frame=[&] {
        ui.begin_frame();
        ImGui::GetIO().DisplaySize={1280,720};
        ImGui::GetIO().DisplayFramebufferScale={1,1};
        if (panel.draw(session)) { camera=session.scene().camera(); }
        session.advance(0);
        renderer.draw(1280,720,camera.projection(1280.F/720.F)*camera.view(),session.scene().world());
        ui.draw(profiler,true);
        ui.render();
        REQUIRE(ImGui::GetDrawData()->TotalVtxCount>0);
        REQUIRE(glGetError()==GL_NO_ERROR);
    };
    for (int i=0;i<3;++i) { frame(); }
    const auto click=[&](float x,float y) {
        for (const bool down:{true,false}) {
            ImGui::GetIO().AddMousePosEvent(x,y);
            ImGui::GetIO().AddMouseButtonEvent(0,down);
            frame();
        }
    };
    click(40,87);
    REQUIRE(session.paused());
    click(120,87);
    REQUIRE(session.ticks()==1);
    frame();
    REQUIRE(session.ticks()==1);
    click(200,87);
    REQUIRE(session.ticks()==0);
    REQUIRE(session.paused());
    if (const auto* path=std::getenv("CORESIM_STUDIO_IMAGE")) {
        std::vector<unsigned char> pixels(1280*720*4);
        glReadPixels(0,0,1280,720,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        std::ofstream image(path,std::ios::binary);
        image << "P6\n1280 720\n255\n";
        for (int y=719;y>=0;--y) {
            for (int x=0;x<1280;++x) {
                image.write(reinterpret_cast<const char*>(pixels.data()+static_cast<std::size_t>((y*1280+x)*4)),3);
            }
        }
        REQUIRE(image.good());
    }
    for (int index=0;index<4;++index) {
        click(210,65);
        frame();
        click(55,85.F+static_cast<float>(index)*17.F);
        const SceneKind expected[]={SceneKind::stress_tower,SceneKind::sphere_rain,SceneKind::particle_storm,SceneKind::collision_demo};
        REQUIRE(session.scene().config().kind==expected[index]);
        REQUIRE(session.paused());
        REQUIRE(session.ticks()==0);
    }
    click(40,87);
    REQUIRE_FALSE(session.paused());
    glDeleteRenderbuffers(1,&depth);
    glDeleteTextures(1,&texture);
    glDeleteFramebuffers(1,&framebuffer);
}
