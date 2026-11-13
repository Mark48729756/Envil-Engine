#pragma once
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace anvil {

struct Camera {
    glm::vec3 position{0.0f, 2.0f, 8.0f};
    float yaw = -90.0f;   // degrees
    float pitch = 0.0f;   // degrees
    float fovY = 60.0f;
    float nearZ = 0.1f, farZ = 500.0f;

    glm::vec3 forward() const {
        float y = glm::radians(yaw), p = glm::radians(pitch);
        return glm::normalize(glm::vec3(std::cos(y) * std::cos(p), std::sin(p), std::sin(y) * std::cos(p)));
    }
    glm::vec3 right() const { return glm::normalize(glm::cross(forward(), glm::vec3(0, 1, 0))); }
    glm::mat4 view() const { return glm::lookAt(position, position + forward(), glm::vec3(0, 1, 0)); }
    glm::mat4 proj(float aspect) const { return glm::perspective(glm::radians(fovY), aspect, nearZ, farZ); }
};

} // namespace anvil
