#pragma once
#include <coresim/studio/SandboxEditor.hpp>
namespace coresim {
class StudioSession;
class EditorPanel {
  public:
    bool draw(StudioSession& session, Camera& camera);
    bool analyze{};
    float sensitivity{1};
    double submission_ms{};

  private:
    void viewport(SandboxEditor& editor, Camera& camera);
    glm::vec3 impulse_{5, 0, 0};
    glm::vec3 focus_{0, 1, 0};
    int tool_{}, axis_{-1};
    bool local_{};
    glm::vec2 start_{}, screen_axis_{};
    glm::vec3 direction_{};
    float units_{};
    ObjectSpec baseline_;
    Entity drag_id_;
    std::string error_;
    char file_[384] = "experiment.coresim.json";
    float frame_history_[90]{};
    int history_pos_{};
};
} // namespace coresim
