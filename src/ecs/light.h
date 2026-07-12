
#pragma once

#include "component.h"
#include "glm/ext/vector_float3.hpp"
#include "imgui.h"
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
};
