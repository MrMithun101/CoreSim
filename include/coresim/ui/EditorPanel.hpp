#pragma once
#include <coresim/studio/SandboxEditor.hpp>
namespace coresim {
class StudioSession;
class EditorPanel {
public:
    bool draw(StudioSession& session, Camera& camera);
    bool analyze{};
private:
    void viewport(SandboxEditor& editor, Camera& camera);
    int tool_{}, axis_{-1}; bool local_{};
    glm::vec2 start_{},screen_axis_{};
    glm::vec3 direction_{};
    float units_{};
    ObjectSpec baseline_;
    Entity drag_id_;
    std::string error_;
};
}
