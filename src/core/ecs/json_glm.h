#pragma once

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

namespace glm {
inline void to_json(nlohmann::json &j, const vec3 &v) { j = {v.x, v.y, v.z}; }
inline void from_json(const nlohmann::json &j, vec3 &v) {
  v.x = j[0];
  v.y = j[1];
  v.z = j[2];
}
inline void to_json(nlohmann::json &j, const vec4 &v) {
  j = {v.x, v.y, v.z, v.w};
}
inline void from_json(const nlohmann::json &j, vec4 &v) {
  v.x = j[0];
  v.y = j[1];
  v.z = j[2];
  v.w = j[3];
}
} // namespace glm