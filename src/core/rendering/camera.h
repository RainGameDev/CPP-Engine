#pragma once

#include "ecs/component.h"
#include "ecs/components.h"
#include <glm/glm.hpp>

class Camera : public Component<Camera> {
private:
  glm::vec3 front;
  glm::vec3 up;
  glm::vec3 right;
  glm::vec3 worldUp;

  float movementSpeed;
  float mouseSensitivity;
  float fov;

public:
  Camera(glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f));

  void updateCameraVectors(const TransformComponent &transform);

  void processKeyboard(TransformComponent &transform, glm::vec3 inputDirection,
                       float deltaTime);
  void processMouseMovement(TransformComponent &transform, float xOffset,
                            float yOffset, bool constrainPitch = true);

  glm::mat4 getViewMatrix(const TransformComponent &transform) const;
  glm::mat4 getProjectionMatrix(float aspectRatio, float nearPlane = 0.01f,
                                float farPlane = 10000.0f) const;

  glm::vec3 getPosition(const TransformComponent &transform) const {
    return transform.position;
  }
  glm::vec3 getFront() const { return front; }
  float getZoom() const { return fov; }

  void inspect(EntityId) {
    ImGui::SeparatorText("Camera");

    ImGui::DragFloat("FOV", &fov, 0.1f);
  }
};

// Editor tool-state camera: lives as a World resource rather than a scene
// entity, so it survives scene resets and is never serialized to a scene
// file.
struct EditorCamera {
  Camera cam;
  TransformComponent transform;
};
