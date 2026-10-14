#pragma once
#include <string>
namespace coresim {
class StudioSession;
class StudioPanel {
public:
    // Called after ImGui NewFrame, before simulation update. True resets the camera.
    bool draw(StudioSession& session);
private:
    std::string error_;
};
}
