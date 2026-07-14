#include "components.h"
#include "light.h"
#include "rendering/camera.h"
#include "serialize.h"

using json = nlohmann::json;

void to_json(json &j, const NameComponent &c) { j["name"] = c.name; }
void from_json(const json &j, NameComponent &c) {
  j.at("name").get_to(c.name);
}

void to_json(json &j, const MeshComponent &c) {
  j["overrideColor"] = {c.overrideColor.x, c.overrideColor.y,
                         c.overrideColor.z, c.overrideColor.w};
}
void from_json(const json &j, MeshComponent &c) {
  auto &col = j.at("overrideColor");
  c.overrideColor = {col[0], col[1], col[2], col[3]};
}

void to_json(json &j, const TransformComponent &c) {
  j["position"] = {c.position.x, c.position.y, c.position.z};
  j["rotation"] = {c.rotation.x, c.rotation.y, c.rotation.z};
  j["scale"] = {c.scale.x, c.scale.y, c.scale.z};
}
void from_json(const json &j, TransformComponent &c) {
  auto &p = j.at("position");
  c.position = {p[0], p[1], p[2]};
  auto &r = j.at("rotation");
  c.rotation = {r[0], r[1], r[2]};
  auto &s = j.at("scale");
  c.scale = {s[0], s[1], s[2]};
}

void to_json(json &j, const Camera &c) { j["fov"] = c.getZoom(); }
void from_json(const json &j, Camera &c) {
  // Camera fov is private, set via inspect only
}

void to_json(json &j, const LightComponent &c) {
  j["intensity"] = c.intensity;
  j["color"] = {c.color.x, c.color.y, c.color.z};
  j["isEmitting"] = c.isEmitting;
  j["lightType"] = c.lightType.index();
}
void from_json(const json &j, LightComponent &c) {
  j.at("intensity").get_to(c.intensity);
  auto &col = j.at("color");
  c.color = {col[0], col[1], col[2]};
  j.at("isEmitting").get_to(c.isEmitting);
}

REGISTER_COMPONENT(NameComponent);
REGISTER_COMPONENT(MeshComponent);
REGISTER_COMPONENT(TransformComponent);
REGISTER_COMPONENT(Camera);
REGISTER_COMPONENT(LightComponent);
