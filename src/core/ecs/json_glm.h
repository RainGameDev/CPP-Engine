#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <nlohmann/json.hpp>

namespace glm {
inline void to_json(nlohmann::json &j, const vec2 &v) { j = {v.x, v.y}; }
inline void from_json(const nlohmann::json &j, vec2 &v) {
  v.x = j[0];
  v.y = j[1];
}
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
inline void to_json(nlohmann::json &j, const quat &v) {
  j = {v.x, v.y, v.z, v.w};
}
inline void from_json(const nlohmann::json &j, quat &v) {
  if (j.is_array() && j.size() >= 4) {
    v.x = j[0];
    v.y = j[1];
    v.z = j[2];
    v.w = j[3];
  } else if (j.is_array() && j.size() == 3) {
    // Legacy scenes stored rotation as euler angles (radians).
    v = quat(vec3{j[0], j[1], j[2]});
  } else {
    v = quat(1.0f, 0.0f, 0.0f, 0.0f);
  }
}
} // namespace glm