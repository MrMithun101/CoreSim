#include <coresim/ui/EditorPanel.hpp>
#include <coresim/studio/StudioSession.hpp>
#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <array>
#include <cmath>
namespace coresim {
namespace {
constexpr const char* shapes[]={"Cube","Sphere","Platform","Ramp","Wall","Ground plane"};
constexpr const char* finishes[]={"Steel","Concrete","Wood","Rubber","Plastic"};
ImVec2 xy(glm::vec2 v){return {v.x,v.y};}
void dock(const char* title,ImVec2 pos,ImVec2 size){
    ImGui::SetNextWindowPos(pos);ImGui::SetNextWindowSize(size);
    ImGui::Begin(title,nullptr,ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings);
}
void frame(Camera& camera,glm::vec3 target,float radius){
    const auto d=camera.direction();
    camera=Camera(target-d*std::clamp(radius*2.6F,2.F,100.F),std::atan2(d.z,d.x),std::asin(d.y));
}
}
void EditorPanel::viewport(SandboxEditor& editor,Camera& cam){
    auto& io=ImGui::GetIO();const glm::vec2 size{io.DisplaySize.x,io.DisplaySize.y},mouse{io.MousePos.x,io.MousePos.y};
    if(size.x<1||size.y<1)return;
    auto vp=cam.projection(size.x/size.y)*cam.view();
    auto project=[&](glm::vec3 p)->std::optional<glm::vec2>{
        const auto c=vp*glm::vec4(p,1);if(c.w<.1F)return {};
        return glm::vec2{(1+c.x/c.w)*size.x*.5F,(1-c.y/c.w)*size.y*.5F};
    };
    auto* draw=ImGui::GetBackgroundDrawList();
    const bool click=ImGui::IsMouseClicked(0)&&!io.WantCaptureMouse&&!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)&&!ImGui::IsMouseDown(1);
    if(!ImGui::IsMouseDown(0))axis_=-1;
    if(auto* item=editor.selected()){
        const auto* t=editor.world().transform(item->entity);
        auto center=project(t->position);
        auto line=[&](glm::vec3 a,glm::vec3 b){auto u=project(a),v=project(b);if(u&&v)draw->AddLine(xy(*u),xy(*v),IM_COL32(91,196,190,185),1.5F);};
        for(int bits=0;bits<8;++bits)for(int k=0;k<3;++k)if(!(bits&(1<<k))){
            glm::vec3 a{bits&1?1.F:-1.F,bits&2?1.F:-1.F,bits&4?1.F:-1.F};auto b=a;b[k]=1;
            line(t->position+t->rotation*(a*t->scale),t->position+t->rotation*(b*t->scale));
        }
        if(center && editor.building() && tool_!=0){
            float len=std::clamp(glm::distance(cam.position(),t->position)*.12F,.3F,15.F);
            float nearest=9.F;int hover=-1;glm::vec2 screen{};glm::vec3 dir{};float factor{};
            constexpr ImU32 colors[]={IM_COL32(226,112,104,255),IM_COL32(131,203,146,255),IM_COL32(104,164,224,255)};
            for(int k=0;k<3;++k){
                glm::vec3 axis(0);axis[k]=1;if(local_||tool_==3)axis=t->rotation*axis;
                auto tip=project(t->position+len*axis);if(!tip)continue;
                auto delta=*tip-*center;auto pixels=glm::length(delta);if(pixels<8)continue;
                auto unit=delta/pixels;
                float dist=glm::distance(mouse,*center+unit*std::clamp(glm::dot(mouse-*center,unit),pixels*.2F,pixels));
                if(dist<nearest){nearest=dist;hover=k;screen=unit;dir=axis;factor=len/pixels;}
                draw->AddLine(xy(*center),xy(*tip),colors[k],3);
                draw->AddCircleFilled(xy(*tip),4,colors[k]);draw->AddText(xy(*tip+glm::vec2(4,-8)),colors[k],k==0?"X":k==1?"Y":"Z");
            }
            if(click&&hover>=0){axis_=hover;start_=mouse;screen_axis_=screen;direction_=dir;units_=factor;baseline_=item->authored;drag_id_=item->entity;return;}
            if(axis_>=0&&item->entity==drag_id_){
                auto o=baseline_;const float pixels=glm::dot(mouse-start_,screen_axis_);
                if(tool_==1)o.pose.position+=direction_*pixels*units_;
                if(tool_==2)o.pose.rotation=glm::normalize(glm::angleAxis(pixels*.015F,direction_)*baseline_.pose.rotation);
                if(tool_==3){
                    const int k=o.primitive==Primitive::sphere?0:axis_;
                    o.pose.scale[k]=std::clamp(o.pose.scale[k]+pixels*units_,.05F,100.F);
                }
                editor.change(o);return;
            }
        }
    }
    if(click){editor.pick(camera_ray(cam,{2*mouse.x/size.x-1,1-2*mouse.y/size.y},size.x/size.y));}
}
bool EditorPanel::draw(StudioSession& session,Camera& camera){
    auto& e=*session.scene().editor();auto size=ImGui::GetIO().DisplaySize;bool reset_camera=false;
    auto edit=[&](auto action){try{action();error_.clear();}catch(const std::exception& ex){error_=ex.what();}};
    try {
        dock("CoreSim Studio",{8,8},{std::max(320.F,size.x-16),65});
        if(ImGui::BeginCombo("##scene", "Sandbox",ImGuiComboFlags_WidthFitPreview)){
            for(auto name:{"collision-demo","stress-tower","sphere-rain","particle-storm"}){
                if(ImGui::Selectable(name)){session.select(scene_config(name));ImGui::EndCombo();ImGui::End();axis_=-1;return true;}
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();ImGui::TextDisabled("|");ImGui::SameLine();
        if(ImGui::Button(e.building()?"Build *":"Build / reset"))session.reset();
        ImGui::SameLine();if(ImGui::Button(session.paused()?"Play":"Pause"))session.set_paused(!session.paused());
        ImGui::SameLine();ImGui::BeginDisabled(!session.paused());if(ImGui::Button("Step"))session.request_step();ImGui::EndDisabled();
        ImGui::SameLine();ImGui::Checkbox("Analyze",&analyze);ImGui::SameLine();ImGui::TextDisabled("%zu objects  |  %.2f s",e.items().size(),static_cast<double>(session.ticks())/120.);
        ImGui::End();
        dock("Tools",{8,81},{146,335});
        ImGui::BeginDisabled(!e.building());
        for(int i=0;i<4;++i)if(ImGui::Selectable(std::array{"Select","Move","Rotate","Scale"}[static_cast<std::size_t>(i)],tool_==i)){tool_=i;axis_=-1;}
        ImGui::Checkbox("Local axes",&local_);ImGui::SeparatorText("Create");
        for(int i=0;i<6;++i)if(ImGui::Selectable(shapes[i])){
            const auto ray=camera_ray(camera,{0,0},std::max(size.x,1.F)/std::max(size.y,1.F));
            auto point=ray.origin+ray.direction*std::min(e.hit(ray).value_or(8.F),40.F);
            auto o=primitive_spec(static_cast<Primitive>(i),point);o.pose.position.y+=o.pose.scale.y+.02F;
            edit([&]{e.create(o);tool_=1;});
        }
        ImGui::EndDisabled();ImGui::End();
        dock("Inspector",{std::max(162.F,size.x-276),81},{268,std::max(280.F,size.y-90)});
        if(auto* item=e.selected()){
            auto o=item->authored;
            ImGui::PushID(static_cast<int>(item->entity.index));
            ImGui::Text("%s  /  %u",shapes[static_cast<int>(o.primitive)],item->entity.index);
            if(ImGui::Button("Frame"))frame(camera,e.world().transform(item->entity)->position,glm::length(o.pose.scale));
            if(e.building()){
                bool changed=false;
                ImGui::SeparatorText("Transform");
                changed|=ImGui::DragFloat3("Position",glm::value_ptr(o.pose.position),.05F,-1000,1000,"%.2f");
                auto degrees=glm::degrees(glm::eulerAngles(o.pose.rotation));
                if(ImGui::DragFloat3("Rotation",glm::value_ptr(degrees),.5F)){o.pose.rotation=glm::quat(glm::radians(degrees));changed=true;}
                if(o.primitive==Primitive::sphere)changed|=ImGui::DragFloat("Radius",&o.pose.scale.x,.02F,.05F,100);
                else changed|=ImGui::DragFloat3("Half size",glm::value_ptr(o.pose.scale),.02F,.05F,100);
                ImGui::SeparatorText("Body");
                bool stat=o.mass==0;ImGui::BeginDisabled(o.primitive==Primitive::plane);
                if(ImGui::Checkbox("Static",&stat)){o.mass=stat?0.F:1.F;changed=true;}
                ImGui::EndDisabled();ImGui::BeginDisabled(stat);
                changed|=ImGui::DragFloat("Mass (kg)",&o.mass,.1F,.01F,10000);
                changed|=ImGui::Checkbox("Gravity",&o.gravity);
                changed|=ImGui::DragFloat3("m/s",glm::value_ptr(o.velocity),.1F,-100,100);
                changed|=ImGui::DragFloat3("rad/s",glm::value_ptr(o.angular),.1F,-50,50);
                ImGui::EndDisabled();ImGui::SeparatorText("Surface");
                changed|=ImGui::SliderFloat("Friction",&o.friction,0,1);
                changed|=ImGui::SliderFloat("Restitution",&o.restitution,0,1);
                int finish=static_cast<int>(o.finish)-1;
                if(ImGui::Combo("Finish",&finish,finishes,5)){o.finish=static_cast<Finish>(finish+1);changed=true;}
                if(changed)edit([&]{e.change(o);});
                if(ImGui::Button("Duplicate"))edit([&]{e.duplicate();});
                ImGui::SameLine();if(ImGui::Button("Delete"))e.erase();
                if(ImGui::Button("Reset transform"))e.reset_transform();
            } else {
                const auto& p=e.world().transform(item->entity)->position;const auto& v=e.world().rigid_body(item->entity)->velocity;
                ImGui::Text("Position  %.2f  %.2f  %.2f",p.x,p.y,p.z);ImGui::Text("Velocity  %.2f  %.2f  %.2f",v.x,v.y,v.z);
                ImGui::TextWrapped("Build restores your authored experiment.");
            }
            ImGui::PopID();
        }else ImGui::TextWrapped("Select a body in the viewport.");
        if(!error_.empty())ImGui::TextWrapped("%s",error_.c_str());
        ImGui::End();
        viewport(e,camera);
    } catch(const std::invalid_argument& err){error_=err.what();}
    return reset_camera;
}
}
