
#pragma once

#include "glm/ext/vector_float3.hpp"
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

struct LightComponent {
public:
  LightType lightType;
  float intensity;
  glm::vec3 color;

  bool isEmitting;
};
