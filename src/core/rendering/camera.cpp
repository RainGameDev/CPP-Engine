#include "camera.h"
#include "glm/ext/matrix_transform.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

Camera::Camera(glm::vec3 up)
    : worldUp(up), movementSpeed(2.5f), mouseSensitivity(0.1f), fov(90.0f),
      front(0.0f, 0.0f, -1.0f), right(1.0f, 0.0f, 0.0f), up(0.0f, 1.0f, 0.0f) {}

void Camera::processKeyboard(TransformComponent &transform,
                             glm::vec3 inputDirection, float deltaTime) {
  updateCameraVectors(transform);
  glm::vec3 movement =
      front * inputDirection.z + right * inputDirection.x + up * inputDirection.y;
  transform.position += movement * movementSpeed * deltaTime;
}

void Camera::processMouseMovement(TransformComponent &transform, float xOffset,
                                  float yOffset, bool constrainPitch) {
  xOffset *= mouseSensitivity;
  yOffset *= mouseSensitivity;

  transform.rotation = glm::normalize(
      glm::angleAxis(glm::radians(-xOffset), glm::vec3(0.0f, 1.0f, 0.0f)) *
      transform.rotation);

  if (constrainPitch) {
    updateCameraVectors(transform);
    float pitch = glm::degrees(
        std::asin(std::clamp(front.y, -0.9999f, 0.9999f)));
    float clamped = std::clamp(pitch + yOffset, -89.0f, 89.0f);
    yOffset = clamped - pitch;
  }

  transform.rotation = glm::normalize(
      glm::angleAxis(glm::radians(yOffset), right) * transform.rotation);

  updateCameraVectors(transform);
}

void Camera::updateCameraVectors(const TransformComponent &transform) {
  front = glm::normalize(glm::mat3_cast(transform.rotation) *
                         glm::vec3(0.0f, 0.0f, -1.0f));
  right = glm::normalize(glm::cross(front, worldUp));
  up = glm::normalize(glm::cross(right, front));
}

glm::mat4 Camera::getProjectionMatrix(float aspectRatio, float nearPlane,
                                      float farPlane) const {
  return glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
}

glm::mat4 Camera::getViewMatrix(const TransformComponent &transform) const {
  return glm::lookAt(transform.position, transform.position + front, up);
}
