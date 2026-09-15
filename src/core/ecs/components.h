#pragma once

#include "assets/handle.h"
#include "component.h"
#include "ecs/component_registry.h"
#include "ecs/json_glm.h"
#include "imgui.h"
#include "rendering/mesh.h"
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

struct NameComponent : Component<NameComponent> {
  std::string name{"Entity"};

  void setName(const std::string &str) { name = str; }

  void inspect(World &, EntityId) { ImGui::SeparatorText("Name"); }
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(NameComponent, name)
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

  void inspect(World &, EntityId) {
    ImGui::SeparatorText("Transform");
    ImGui::DragFloat3("Position", &position.x, 0.1f);
    ImGui::DragFloat3("Rotation", &rotation.x, 0.1f);
    ImGui::DragFloat3("Scale", &scale.x, 0.1f);
  }
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(TransformComponent, position, rotation, scale)
};

struct DeltaTime {
  float value{0.0f};
};

REGISTER_COMPONENT(NameComponent);
REGISTER_COMPONENT(MeshComponent);
REGISTER_COMPONENT(TransformComponent);
