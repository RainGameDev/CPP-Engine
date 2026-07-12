#pragma once

#include "component.h"
#include "imgui.h"
#include "rendering/mesh.h"
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <string>

struct NameComponent : Component<NameComponent> {
  std::string name{"Entity"};

  void setName(const std::string &str) { name = str; }

  void inspect(EntityId) {}
};

struct MeshComponent : Component<MeshComponent> {
  Mesh mesh;
  glm::vec4 overrideColor{0.0f};

  void inspect(EntityId) {}
};

struct TransformComponent : Component<TransformComponent> {
  glm::vec3 position{0.0f};
  glm::vec3 rotation{0.0f};
  glm::vec3 scale{1.0f};

  glm::mat4 getMatrix() const {
    glm::mat4 mat(1.0f);
    mat = glm::translate(mat, position);
    mat = glm::rotate(mat, rotation.x, glm::vec3(1, 0, 0));
    mat = glm::rotate(mat, rotation.y, glm::vec3(0, 1, 0));
    mat = glm::rotate(mat, rotation.z, glm::vec3(0, 0, 1));
    mat = glm::scale(mat, scale);
    return mat;
  }

  void inspect(EntityId) {
    ImGui::SeparatorText("Transform");
    ImGui::DragFloat3("Position", &position.x, 0.1f);
    ImGui::DragFloat3("Rotation", &rotation.x, 0.1f);
    ImGui::DragFloat3("Scale", &scale.x, 0.1f);
  }
};
