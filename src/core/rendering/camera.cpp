#include "camera.h"
#include "glm/ext/matrix_transform.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/glm.hpp>

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

  transform.rotation.y += xOffset;
  transform.rotation.x += yOffset;

  if (constrainPitch) {
    transform.rotation.x = std::clamp(transform.rotation.x, -89.0f, 89.0f);
  }

  updateCameraVectors(transform);
}

void Camera::updateCameraVectors(const TransformComponent &transform) {
  float yaw = transform.rotation.y;
  float pitch = transform.rotation.x;

  glm::vec3 newFront;
  newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
  newFront.y = sin(glm::radians(pitch));
  newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
  front = glm::normalize(newFront);

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
