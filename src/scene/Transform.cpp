#include <coresim/scene/Transform.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace coresim {
glm::mat4 Transform::matrix() const {
    return glm::translate(glm::mat4(1.0F), position) * glm::mat4_cast(rotation) *
           glm::scale(glm::mat4(1.0F), scale);
}
} // namespace coresim
