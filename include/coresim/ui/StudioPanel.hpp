#pragma once
#include <string>
#include <coresim/ui/EditorPanel.hpp>
namespace coresim {
class StudioSession;
class StudioPanel {
public:
    // Called after ImGui NewFrame, before simulation update. True resets the camera.
    bool draw(StudioSession& session,Camera& camera);
    void set_submission_ms(double ms){editor_.submission_ms=ms;}
    float sensitivity() const {return editor_.sensitivity;}
    bool analyze() const {return editor_.analyze;}
private:
    EditorPanel editor_;
    std::string error_;
};
}
