
#pragma once

#include "component.h"
#include "ecs/component_registry.h"
#include "ecs/json_glm.h"
#include "glm/ext/vector_float3.hpp"
#include "imgui.h"
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <variant>

struct Spot {
  float angle;
  float length;
};

struct Point {
  float radius;
};

struct Directional {};

using LightType = std::variant<Spot, Point, Directional>;

inline void to_json(nlohmann::json &j, const LightType &l) {
  if (std::holds_alternative<Directional>(l)) {
    j = {0};
  } else if (std::holds_alternative<Point>(l)) {
    j = {1, std::get<Point>(l).radius};
  } else {
    const auto &s = std::get<Spot>(l);
    j = {2, s.angle, s.length};
  }
}
inline void from_json(const nlohmann::json &j, LightType &l) {
  if (j.is_array()) {
    switch (j.at(0).get<int>()) {
    case 0:
      l = Directional{};
      break;
    case 1:
      l = Point{j.at(1).get<float>()};
      break;
    case 2:
      l = Spot{j.at(1).get<float>(), j.at(2).get<float>()};
      break;
    default:
      l = Directional{};
      break;
    }
  } else {
    switch (j.get<int>()) {
    case 0:
      l = Directional{};
      break;
    case 1:
      l = Point{10.0f};
      break;
    case 2:
      l = Spot{45.0f, 20.0f};
      break;
    default:
      l = Directional{};
      break;
    }
  }
}

struct LightComponent : Component<LightComponent> {
public:
  LightType lightType;
  float intensity;
  glm::vec3 color;

  bool isEmitting;

  void inspect(EntityId) {
    ImGui::SeparatorText("Light");
    ImGui::ColorEdit3("Color", &color.x);
    ImGui::DragFloat("Intensity", &intensity, 0.1f, 0.0f, 100.0f);
    ImGui::Checkbox("Emitting", &isEmitting);

    const char *types[] = {"Directional", "Point", "Spot"};
    int currentType = lightType.index();
    if (ImGui::Combo("Type", &currentType, types, 3)) {
      if (currentType == 0)
        lightType = Directional{};
      else if (currentType == 1)
        lightType = Point{10.0f};
      else
        lightType = Spot{45.0f, 20.0f};
    }

    if (std::holds_alternative<Point>(lightType)) {
      ImGui::DragFloat("Radius", &std::get<Point>(lightType).radius, 0.1f, 0.0f,
                       1000.0f);
    } else if (std::holds_alternative<Spot>(lightType)) {
      ImGui::DragFloat("Angle", &std::get<Spot>(lightType).angle, 1.0f, 1.0f,
                       180.0f);
      ImGui::DragFloat("Length", &std::get<Spot>(lightType).length, 0.1f, 0.0f,
                       1000.0f);
    }
  }
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(LightComponent, lightType, intensity, color,
                                 isEmitting);
};

REGISTER_COMPONENT(LightComponent);
