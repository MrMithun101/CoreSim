#include <coresim/ui/StudioPanel.hpp>
#include <coresim/studio/StudioSession.hpp>
#include <imgui.h>
#include <array>
#include <exception>
namespace coresim {
bool StudioPanel::draw(StudioSession& session) {
    bool camera_reset=false;
    ImGui::SetNextWindowPos({12,12},ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({350,270},ImGuiCond_FirstUseEver);
    if (ImGui::Begin("CoreSim Studio")) {
        ImGui::TextUnformatted("Interactive 3D systems-performance laboratory");
        constexpr std::array names{"Tower","Rain","Particle Storm","Sandbox"};
        constexpr std::array kinds{SceneKind::stress_tower,SceneKind::sphere_rain,SceneKind::particle_storm,SceneKind::collision_demo};
        int current=0;
        for (int i=0;i<4;++i) { if (session.scene().config().kind==kinds[static_cast<std::size_t>(i)]) { current=i; } }
        ImGui::SetNextItemWidth(200);
        if (ImGui::Combo("Simulation",&current,names.data(),static_cast<int>(names.size()))) {
            try {
                session.select(scene_config(scene_name(kinds[static_cast<std::size_t>(current)])));
                error_.clear();
                camera_reset=true;
            } catch (const std::exception& error) { error_=error.what(); }
        }
        if (ImGui::Button(session.paused() ? "Resume" : "Pause")) { session.set_paused(!session.paused()); }
        ImGui::SameLine();
        ImGui::BeginDisabled(!session.paused());
        if (ImGui::Button("Single step")) { session.request_step(); }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Reset")) { session.reset(); camera_reset=true; }
        ImGui::Text("%s | %llu ticks | %.3f s",session.paused() ? "Paused" : "Running",
                    static_cast<unsigned long long>(session.ticks()),static_cast<double>(session.ticks())*FixedStepper::step_seconds);
        ImGui::Text("Objects: %zu | Seed: %u",session.scene().config().objects,session.scene().config().seed);
        ImGui::TextWrapped("Single step advances 1/120 s while paused. Switching scenes loads default counts and seed; pause state is preserved.");
        if (session.scene().config().kind==SceneKind::collision_demo) {
            ImGui::TextWrapped("Sandbox: mixed spheres and boxes. Object editing is not available yet.");
        }
        ImGui::Separator();
        ImGui::TextWrapped("WASD move | Q/E down/up | Right-drag look | R reset | Esc close");
        if (!error_.empty()) { ImGui::TextWrapped("Scene change failed: %s",error_.c_str()); }
    }
    ImGui::End();
    return camera_reset;
}
}
