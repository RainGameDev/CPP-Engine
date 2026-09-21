#pragma once

#define GLM_ENABLE_EXPERIMENTAL

#include "assets/handle.h"
#include "component.h"
#include "ecs/component_registry.h"
#include "ecs/json_glm.h"
#include "imgui.h"
#include "rendering/mesh.h"
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

struct NameComponent : Component<NameComponent> {
  std::string name{"Entity"};

  void setName(const std::string &str) { name = str; }

  static constexpr bool showInspectorHeader() { return false; }

  void inspect(World &, EntityId) {}
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(NameComponent, name)
};

struct TransformComponent : Component<TransformComponent> {
  glm::vec3 position{0.0f};
  glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
  glm::vec3 scale{1.0f};

  glm::mat4 getMatrix() const {
    return glm::translate(glm::mat4(1.0f), position) *
           glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);
  }

  void inspect(World &, EntityId id) {
    ImGui::DragFloat3("Position", &position.x, 0.1f);

    struct CachedRotation {
      glm::quat lastDisplayed{1.0f, 0.0f, 0.0f, 0.0f};
      glm::vec3 euler{0.0f};
    };
    static std::unordered_map<EntityId, CachedRotation> cache;
    CachedRotation &cached = cache[id];

    if (cached.lastDisplayed != rotation) {
      cached.lastDisplayed = rotation;
      cached.euler = glm::degrees(glm::eulerAngles(rotation));
    }
    if (ImGui::DragFloat3("Rotation", &cached.euler.x, 0.1f)) {
      rotation = glm::quat(glm::radians(cached.euler));
      cached.lastDisplayed = rotation;
      cached.euler = glm::degrees(glm::eulerAngles(rotation));
    }

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
