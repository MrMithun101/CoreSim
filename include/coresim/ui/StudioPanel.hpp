#pragma once
#include <string>
#include <coresim/ui/EditorPanel.hpp>
namespace coresim {
class StudioSession;
class StudioPanel {
public:
    // Called after ImGui NewFrame, before simulation update. True resets the camera.
    bool draw(StudioSession& session,Camera& camera);
    bool analyze() const {return editor_.analyze;}
private:
    EditorPanel editor_;
    std::string error_;
};
}
