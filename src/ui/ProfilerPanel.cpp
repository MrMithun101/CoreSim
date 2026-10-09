#include <coresim/ui/ProfilerPanel.hpp>
#include <coresim/core/Window.hpp>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <stdexcept>

namespace coresim {
ProfilerPanel::ProfilerPanel(Window& window) {
    if (ImGui::GetCurrentContext()) { throw std::logic_error("Only one profiler UI context is supported"); }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().LogFilename = nullptr;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    if (!ImGui_ImplGlfw_InitForOpenGL(window.native_handle(), true)) {
        ImGui::DestroyContext();
        throw std::runtime_error("ImGui GLFW initialization failed");
    }
    if (!ImGui_ImplOpenGL3_Init("#version 330 core")) {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        throw std::runtime_error("ImGui OpenGL initialization failed");
    }
}
ProfilerPanel::~ProfilerPanel() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}
void ProfilerPanel::begin_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}
bool ProfilerPanel::captures_keyboard() const { return ImGui::GetIO().WantCaptureKeyboard; }
bool ProfilerPanel::captures_mouse() const { return ImGui::GetIO().WantCaptureMouse; }
void ProfilerPanel::draw(Profiler& profiler) {
    ImGui::SetNextWindowPos({12, 12}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({710, 340}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("CPU Profiler")) {
        bool enabled = profiler.enabled();
        if (ImGui::Checkbox("Collect samples", &enabled)) {
            profiler.set_enabled(enabled);
            next_refresh_ = 0;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset statistics")) { profiler.reset(); next_refresh_ = 0; }
        ImGui::SameLine();
        ImGui::TextUnformatted(enabled ? "Live (4 Hz refresh)" : "Paused; simulation continues");
        if (ImGui::GetTime() >= next_refresh_) {
            snapshot_ = profiler.stats();
            next_refresh_ = ImGui::GetTime() + 0.25;
        }
        ImGui::TextUnformatted("CPU wall time in ms | cumulative since reset | nested/inclusive scopes");
        if (ImGui::BeginTable("ProfileStats", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                                   ImGuiTableFlags_SizingStretchProp)) {
            for (const char* name : {"System", "Calls", "Total ms", "Avg ms", "Min ms", "Max ms"}) {
                ImGui::TableSetupColumn(name);
            }
            ImGui::TableHeadersRow();
            for (std::size_t i = 0; i < snapshot_.size(); ++i) {
                const auto& entry = snapshot_[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted(profile_names[i].data());
                ImGui::TableNextColumn(); ImGui::Text("%llu", static_cast<unsigned long long>(entry.count));
                ImGui::TableNextColumn(); ImGui::Text("%.3f", entry.total_ms);
                ImGui::TableNextColumn(); ImGui::Text("%.3f", entry.average_ms());
                ImGui::TableNextColumn(); ImGui::Text("%.3f", entry.min_ms);
                ImGui::TableNextColumn(); ImGui::Text("%.3f", entry.max_ms);
            }
            ImGui::EndTable();
        }
        ImGui::TextWrapped("Frame includes events and presentation. Rendering includes scene + UI CPU submission; "
                           "Presentation includes VSync waiting. Physics/phases count fixed ticks, not frames. "
                           "The current frame appears after completion. These are not GPU timings.");
    }
    ImGui::End();
}
void ProfilerPanel::render() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
} // namespace coresim
