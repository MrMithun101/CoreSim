#include <coresim/studio/SceneFile.hpp>
#include <coresim/studio/StudioSession.hpp>
#include <coresim/ui/EditorPanel.hpp>
#include <imgui.h>
#if defined(__APPLE__) || defined(__linux__)
#include <sys/resource.h>
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <glm/gtc/type_ptr.hpp>
namespace coresim {
namespace {
constexpr const char* shapes[] = {"Cube", "Sphere", "Platform", "Ramp", "Wall", "Ground plane"};
constexpr const char* finishes[] = {"Steel", "Concrete", "Wood", "Rubber", "Plastic"};
ImVec2 xy(glm::vec2 v) { return {v.x, v.y}; }
void dock(const char* title, ImVec2 pos, ImVec2 size) {
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin(title, nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoSavedSettings);
}
void frame(Camera& camera, glm::vec3 target, float radius) {
    const auto d = camera.direction();
    camera = Camera(target - d * std::clamp(radius * 2.6F, 2.F, 100.F), std::atan2(d.z, d.x),
                    std::asin(d.y));
}
} // namespace
void EditorPanel::viewport(SandboxEditor& editor, Camera& cam) {
    auto& io = ImGui::GetIO();
    const glm::vec2 size{io.DisplaySize.x, io.DisplaySize.y}, mouse{io.MousePos.x, io.MousePos.y};
    if (size.x < 1 || size.y < 1)
        return;
    if (!io.WantCaptureMouse && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && axis_ < 0) {
        auto delta = io.MouseDelta;
        auto d = cam.direction();
        float distance = std::clamp(glm::distance(cam.position(), focus_), .7F, 110.F);
        auto place = [&](glm::vec3 pos, glm::vec3 direction) {
            pos.y = std::max(pos.y, .18F);
            cam = Camera(pos, std::atan2(direction.z, direction.x),
                         std::asin(std::clamp(direction.y, -1.F, 1.F)));
        };
        if (io.KeyAlt && ImGui::IsMouseDragging(0)) {
            const auto offset = focus_ - cam.position();
            if (glm::length(offset) > .001F)
                d = glm::normalize(offset);
            Camera orbit({}, std::atan2(d.z, d.x), std::asin(d.y));
            orbit.look(delta.x * .006F * sensitivity, -delta.y * .006F * sensitivity);
            d = orbit.direction();
            place(focus_ - d * distance, d);
        } else if (ImGui::IsMouseDragging(2)) {
            auto right = glm::normalize(glm::cross(d, glm::vec3(0, 1, 0)));
            auto up = glm::cross(right, d);
            auto shift = (up * delta.y - right * delta.x) * (distance / size.y) * sensitivity;
            focus_ += shift;
            place(cam.position() + shift, d);
        } else if (io.MouseWheel != 0) {
            auto toward = focus_ - cam.position();
            if (glm::length(toward) > .001F)
                toward = glm::normalize(toward);
            else
                toward = d;
            float next =
                std::clamp(distance * std::exp(-io.MouseWheel * .16F * sensitivity), .7F, 110.F);
            place(cam.position() + toward * (distance - next), d);
        }
    }
    auto vp = cam.projection(size.x / size.y) * cam.view();
    auto project = [&](glm::vec3 p) -> std::optional<glm::vec2> {
        const auto c = vp * glm::vec4(p, 1);
        if (c.w < .1F)
            return {};
        return glm::vec2{(1 + c.x / c.w) * size.x * .5F, (1 - c.y / c.w) * size.y * .5F};
    };
    auto* draw = ImGui::GetBackgroundDrawList();
    const bool click = !io.KeyAlt && ImGui::IsMouseClicked(0) && !io.WantCaptureMouse &&
                       !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) &&
                       !ImGui::IsMouseDown(1);
    if (!ImGui::IsMouseDown(0))
        axis_ = -1;
    if (auto* item = editor.selected()) {
        const auto* t = editor.world().transform(item->entity);
        auto center = project(t->position);
        auto line = [&](glm::vec3 a, glm::vec3 b) {
            auto u = project(a), v = project(b);
            if (u && v)
                draw->AddLine(xy(*u), xy(*v), IM_COL32(91, 196, 190, 185), 1.5F);
        };
        for (int bits = 0; bits < 8; ++bits)
            for (int k = 0; k < 3; ++k)
                if (!(bits & (1 << k))) {
                    glm::vec3 a{bits & 1 ? 1.F : -1.F, bits & 2 ? 1.F : -1.F,
                                bits & 4 ? 1.F : -1.F};
                    auto b = a;
                    b[k] = 1;
                    line(t->position + t->rotation * (a * t->scale),
                         t->position + t->rotation * (b * t->scale));
                }
        if (center && editor.building() && tool_ != 0) {
            float len = std::clamp(glm::distance(cam.position(), t->position) * .12F, .3F, 15.F);
            float nearest = 9.F;
            int hover = -1;
            glm::vec2 screen{};
            glm::vec3 dir{};
            float factor{};
            constexpr ImU32 colors[] = {IM_COL32(226, 112, 104, 255), IM_COL32(131, 203, 146, 255),
                                        IM_COL32(104, 164, 224, 255)};
            for (int k = 0; k < 3; ++k) {
                glm::vec3 axis(0);
                axis[k] = 1;
                if (local_ || tool_ == 3)
                    axis = t->rotation * axis;
                auto tip = project(t->position + len * axis);
                if (!tip)
                    continue;
                auto delta = *tip - *center;
                auto pixels = glm::length(delta);
                if (pixels < 8)
                    continue;
                auto unit = delta / pixels;
                float dist = glm::distance(
                    mouse, *center + unit * std::clamp(glm::dot(mouse - *center, unit),
                                                       pixels * .2F, pixels));
                if (dist < nearest) {
                    nearest = dist;
                    hover = k;
                    screen = unit;
                    dir = axis;
                    factor = len / pixels;
                }
                draw->AddLine(xy(*center), xy(*tip), colors[k], 3);
                draw->AddCircleFilled(xy(*tip), 4, colors[k]);
                draw->AddText(xy(*tip + glm::vec2(4, -8)), colors[k],
                              k == 0   ? "X"
                              : k == 1 ? "Y"
                                       : "Z");
            }
            if (click && hover >= 0) {
                axis_ = hover;
                start_ = mouse;
                screen_axis_ = screen;
                direction_ = dir;
                units_ = factor;
                baseline_ = item->authored;
                drag_id_ = item->entity;
                return;
            }
            if (axis_ >= 0 && item->entity == drag_id_) {
                auto o = baseline_;
                const float pixels = glm::dot(mouse - start_, screen_axis_);
                if (tool_ == 1)
                    o.pose.position += direction_ * pixels * units_;
                if (tool_ == 2)
                    o.pose.rotation = glm::normalize(glm::angleAxis(pixels * .015F, direction_) *
                                                     baseline_.pose.rotation);
                if (tool_ == 3) {
                    const int k = o.primitive == Primitive::sphere ? 0 : axis_;
                    o.pose.scale[k] = std::clamp(o.pose.scale[k] + pixels * units_, .05F, 100.F);
                }
                editor.change(o);
                return;
            }
        }
    }
    if (click) {
        editor.pick(
            camera_ray(cam, {2 * mouse.x / size.x - 1, 1 - 2 * mouse.y / size.y}, size.x / size.y));
    }
}
bool EditorPanel::draw(StudioSession& session, Camera& camera) {
    auto& e = *session.scene().editor();
    auto size = ImGui::GetIO().DisplaySize;
    bool reset_camera = false;
    auto edit = [&](auto action) {
        try {
            action();
            error_.clear();
        } catch (const std::exception& ex) {
            error_ = ex.what();
        }
    };
    struct Theme {
        Theme() {
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(.062F, .077F, .085F, .97F));
            ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(.10F, .16F, .17F, 1));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.15F, .25F, .26F, 1));
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(.13F, .29F, .29F, 1));
            ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(.45F, .82F, .76F, 1));
        }
        ~Theme() { ImGui::PopStyleColor(5); }
    } theme;
    try {
        if (ImGui::IsMouseClicked(0))
            e.begin_gesture();
        dock("CoreSim Studio", {8, 8}, {std::max(320.F, size.x - 16), 65});
        if (ImGui::BeginCombo("##scene",
                              session.scene().config().kind == SceneKind::structural_lab
                                  ? "Structural Lab"
                                  : "Sandbox",
                              ImGuiComboFlags_WidthFitPreview)) {
            for (auto name : {"structural-lab", "sandbox", "collision-demo", "stress-tower",
                              "sphere-rain", "particle-storm"}) {
                if (ImGui::Selectable(name)) {
                    try {
                        session.select(scene_config(name));
                        ImGui::EndCombo();
                        ImGui::End();
                        axis_ = -1;
                        return true;
                    } catch (const std::exception& ex) {
                        error_ = ex.what();
                    }
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        if (ImGui::Button(e.building() ? "Build *" : "Build / reset"))
            session.reset();
        ImGui::SameLine();
        if (ImGui::Button(session.paused() ? "Play" : "Pause"))
            session.set_paused(!session.paused());
        ImGui::SameLine();
        ImGui::BeginDisabled(!session.paused());
        if (ImGui::Button("Step"))
            session.request_step();
        ImGui::EndDisabled();
        if (e.projectile() != Entity{}) {
            ImGui::SameLine();
            ImGui::BeginDisabled(!e.launch_ready());
            if (ImGui::Button("Launch sphere")) {
                edit([&] {
                    session.set_paused(false);
                    e.launch();
                });
            }
            ImGui::EndDisabled();
        }
        ImGui::SameLine();
        ImGui::Checkbox("Analyze", &analyze);
        ImGui::SameLine();
        ImGui::TextDisabled("%zu objects  |  %.2f s", e.items().size(),
                            static_cast<double>(session.ticks()) / 120.);
        ImGui::SameLine();
        if (ImGui::Button("Frame all")) {
            glm::vec3 lo(10000), hi(-10000);
            bool any = false;
            for (const auto& item : e.items())
                if (item.authored.primitive != Primitive::plane) {
                    auto* t = e.world().transform(item.entity);
                    auto r = glm::vec3(glm::length(t->scale));
                    lo = glm::min(lo, t->position - r);
                    hi = glm::max(hi, t->position + r);
                    any = true;
                }
            focus_ = any ? (lo + hi) * .5F : glm::vec3(0);
            frame(camera, focus_, any ? glm::length(hi - lo) * .5F : 6.F);
        }
        ImGui::SameLine();
        if (ImGui::Button("File"))
            ImGui::OpenPopup("Scene file");
        if (ImGui::BeginPopup("Scene file")) {
            ImGui::SetNextItemWidth(330);
            ImGui::InputText("Path", file_, sizeof(file_));
            ImGui::TextDisabled("Save replaces this file with your Build state.");
            ImGui::BeginDisabled(!e.building());
            if (ImGui::Button("Save"))
                edit([&] {
                    write_scene_file(file_, session.scene().config().kind, e, camera);
                    error_ = "Scene saved";
                    ImGui::CloseCurrentPopup();
                });
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Load")) {
                try {
                    auto doc = read_scene_file(file_);
                    session.load(doc);
                    camera = doc.camera;
                    axis_ = -1;
                    focus_ = camera.position() + camera.direction() * 10.F;
                    error_ = "Scene loaded";
                    ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                    ImGui::End();
                    return false;
                } catch (const std::exception& ex) {
                    error_ = ex.what();
                }
            }
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!e.can_undo());
        if (ImGui::Button("Undo"))
            e.undo();
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!e.can_redo());
        if (ImGui::Button("Redo"))
            e.redo();
        ImGui::EndDisabled();
        ImGui::End();
        dock("Tools", {8, 81}, {146, 365});
        ImGui::BeginDisabled(!e.building());
        for (int i = 0; i < 4; ++i)
            if (ImGui::Selectable(
                    std::array{"Select", "Move", "Rotate", "Scale"}[static_cast<std::size_t>(i)],
                    tool_ == i)) {
                tool_ = i;
                axis_ = -1;
            }
        ImGui::Checkbox("Local axes", &local_);
        ImGui::SeparatorText("Create");
        for (int i = 0; i < 6; ++i)
            if (ImGui::Selectable(shapes[i])) {
                const auto ray =
                    camera_ray(camera, {0, 0}, std::max(size.x, 1.F) / std::max(size.y, 1.F));
                auto point = ray.origin + ray.direction * std::min(e.hit(ray).value_or(8.F), 40.F);
                auto o = primitive_spec(static_cast<Primitive>(i), point);
                o.pose.position.y += o.pose.scale.y + .02F;
                edit([&] {
                    e.create(o);
                    tool_ = 1;
                });
            }
        ImGui::EndDisabled();
        ImGui::SeparatorText("Navigation");
        ImGui::SetNextItemWidth(120);
        ImGui::SliderFloat("##sensitivity", &sensitivity, .35F, 2.5F, "Gain %.2f");
        ImGui::TextDisabled("Alt-drag orbit");
        ImGui::TextDisabled("Middle-drag pan");
        ImGui::End();
        dock("Inspector", {std::max(162.F, size.x - 276), 81}, {268, std::max(280.F, size.y - 90)});
        if (auto* item = e.selected()) {
            auto o = item->authored;
            ImGui::PushID(static_cast<int>(item->entity.index));
            ImGui::Text("%s  /  %u", shapes[static_cast<int>(o.primitive)], item->entity.index);
            if (ImGui::Button("Frame")) {
                focus_ = e.world().transform(item->entity)->position;
                frame(camera, focus_, glm::length(o.pose.scale));
            }
            if (e.building()) {
                bool changed = false;
                ImGui::SeparatorText("Transform");
                changed |= ImGui::DragFloat3("Position", glm::value_ptr(o.pose.position), .05F,
                                             -1000, 1000, "%.2f");
                auto degrees = glm::degrees(glm::eulerAngles(o.pose.rotation));
                if (ImGui::DragFloat3("Rotation", glm::value_ptr(degrees), .5F)) {
                    o.pose.rotation = glm::quat(glm::radians(degrees));
                    changed = true;
                }
                if (o.primitive == Primitive::sphere)
                    changed |= ImGui::DragFloat("Radius", &o.pose.scale.x, .02F, .05F, 100);
                else
                    changed |= ImGui::DragFloat3("Half size", glm::value_ptr(o.pose.scale), .02F,
                                                 .05F, 100);
                ImGui::SeparatorText("Body");
                bool stat = o.mass == 0;
                ImGui::BeginDisabled(o.primitive == Primitive::plane);
                if (ImGui::Checkbox("Static", &stat)) {
                    o.mass = stat ? 0.F : 1.F;
                    changed = true;
                }
                ImGui::EndDisabled();
                ImGui::BeginDisabled(stat);
                changed |= ImGui::DragFloat("Mass (kg)", &o.mass, .1F, .01F, 10000);
                changed |= ImGui::Checkbox("Gravity", &o.gravity);
                changed |= ImGui::DragFloat3("m/s", glm::value_ptr(o.velocity), .1F, -100, 100);
                changed |= ImGui::DragFloat3("rad/s", glm::value_ptr(o.angular), .1F, -50, 50);
                ImGui::EndDisabled();
                if (item->entity == e.projectile())
                    ImGui::DragFloat3("Launch m/s", glm::value_ptr(e.launch_velocity), .1F, -50, 50,
                                      "%.1f", ImGuiSliderFlags_AlwaysClamp);
                ImGui::SeparatorText("Surface");
                changed |= ImGui::SliderFloat("Friction", &o.friction, 0, 1);
                changed |= ImGui::SliderFloat("Restitution", &o.restitution, 0, 1);
                int finish = static_cast<int>(o.finish) - 1;
                if (ImGui::Combo("Finish", &finish, finishes, 5)) {
                    o.finish = static_cast<Finish>(finish + 1);
                    changed = true;
                }
                if (changed)
                    edit([&] { e.change(o); });
                if (ImGui::Button("Duplicate"))
                    edit([&] { e.duplicate(); });
                ImGui::SameLine();
                if (ImGui::Button("Delete"))
                    e.erase();
                if (ImGui::Button("Reset transform"))
                    e.reset_transform();
            } else {
                const auto& p = e.world().transform(item->entity)->position;
                const auto& v = e.world().rigid_body(item->entity)->velocity;
                ImGui::Text("Position  %.2f  %.2f  %.2f", p.x, p.y, p.z);
                ImGui::Text("Velocity  %.2f  %.2f  %.2f", v.x, v.y, v.z);
                const auto* body = e.world().rigid_body(item->entity);
                auto av = body->angular_velocity;
                ImGui::Text("Angular  %.2f  %.2f  %.2f", av.x, av.y, av.z);
                ImGui::Text("%s  |  %.2f kg",
                            o.mass == 0        ? "Static"
                            : body->sleeping() ? "Sleeping"
                                               : "Dynamic",
                            o.mass);
                ImGui::TextWrapped("Build restores your authored experiment.");
                ImGui::BeginDisabled(o.mass == 0);
                ImGui::DragFloat3("Impulse N s", glm::value_ptr(impulse_), .1F, -100, 100, "%.1f",
                                  ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::Button("Apply impulse"))
                    edit([&] { e.impulse(impulse_); });
                ImGui::EndDisabled();
            }
            ImGui::PopID();
        } else
            ImGui::TextWrapped("Select a body in the viewport.");
        if (!error_.empty())
            ImGui::TextWrapped("%s", error_.c_str());
        ImGui::End();
        session.collect_analysis(analyze);
        if (analyze) {
            dock("Analysis", {162, std::max(82.F, size.y - 150)},
                 {std::max(270.F, size.x - 446), 142});
            auto& stats = session.collision_stats();
            auto& io = ImGui::GetIO();
            frame_history_[history_pos_] = io.DeltaTime * 1000;
            history_pos_ = (history_pos_ + 1) % 90;
            ImGui::Text("%.1f FPS   |   frame %.2f ms   |   scene submission %.2f ms", io.Framerate,
                        io.DeltaTime * 1000, submission_ms);
            ImGui::Text("Last tick %.2f ms   |   broad %.2f ms   |   solver %.2f ms",
                        session.physics_ms(), stats.broad_phase_ms, stats.solver_ms);
            ImGui::Text("%llu candidates   |   %llu contact points",
                        static_cast<unsigned long long>(stats.candidate_pairs),
                        static_cast<unsigned long long>(stats.contacts));
#if defined(__APPLE__) || defined(__linux__)
            rusage usage{};
            if (getrusage(RUSAGE_SELF, &usage) == 0) {
#ifdef __APPLE__
                constexpr double unit = 1048576.;
#else
                constexpr double unit = 1024.;
#endif
                ImGui::SameLine();
                ImGui::Text("|  peak RSS %.1f MiB", static_cast<double>(usage.ru_maxrss) / unit);
            }
#endif
            ImGui::PlotLines("Frame ms", frame_history_, 90, history_pos_, nullptr, 0, 50, {0, 30});
            ImGui::End();
        }
        auto& io = ImGui::GetIO();
        if (!io.WantTextInput && !ImGui::IsAnyItemActive() &&
            !ImGui::IsPopupOpen(nullptr,
                                ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) &&
            !ImGui::IsMouseDown(1)) {
            const bool mod = io.KeyCtrl || io.KeySuper;
            if (ImGui::IsKeyPressed(ImGuiKey_Space, false))
                session.set_paused(!session.paused());
            if (e.building()) {
                if (mod && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
                    if (io.KeyShift)
                        e.redo();
                    else
                        e.undo();
                }
                if (mod && ImGui::IsKeyPressed(ImGuiKey_D, false))
                    edit([&] { e.duplicate(); });
                if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) ||
                    ImGui::IsKeyPressed(ImGuiKey_Backspace, false))
                    e.erase();
                for (int k = 0; k < 4; ++k)
                    if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_1 + k), false))
                        tool_ = k;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_F, false) && e.selected()) {
                focus_ = e.world().transform(e.selected()->entity)->position;
                frame(camera, focus_, glm::length(e.selected()->authored.pose.scale));
            }
        }
        viewport(e, camera);
        if (!ImGui::IsMouseDown(0) && !ImGui::IsAnyItemActive())
            e.end_gesture();
    } catch (const std::invalid_argument& err) {
        error_ = err.what();
    }
    return reset_camera;
}
} // namespace coresim
