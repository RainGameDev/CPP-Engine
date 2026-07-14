
#pragma once

#include "ecs/entity.h"
#include "ecs/system_registry.h"
#include "ecs/world.h"
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

using json = nlohmann::json;

class ComponentRegistry {
public:
  struct Entry {
    // Returns false if this entity doesn't have the component.
    std::function<bool(World &, EntityId, json &)> save;
    std::function<void(World &, EntityId, const json &)> load;
  };

  static ComponentRegistry &instance() {
    static ComponentRegistry inst;
    return inst;
  }

  template <ComponentType T> void register_component(std::string name) {
    Entry entry;
    entry.save = [](World &world, EntityId id, json &out) -> bool {
      auto *c = world.get_storage<T>().get(id);
      if (!c)
        return false;
      out = *c; // calls to_json(json&, const T&)
      return true;
    };
    entry.load = [](World &world, EntityId id, const json &in) {
      world.add_component<T>(id, in.get<T>());
    };
    entries_[name] = std::move(entry);
  }

  const std::unordered_map<std::string, Entry> &entries() const {
    return entries_;
  }

private:
  std::unordered_map<std::string, Entry> entries_;
};

#define REGISTER_COMPONENT(Type)                                               \
  namespace {                                                                  \
  struct ECS_CONCAT(Type, _component_registrar) {                              \
    ECS_CONCAT(Type, _component_registrar)() {                                 \
      ComponentRegistry::instance().register_component<Type>(#Type);           \
    }                                                                          \
  };                                                                           \
  [[maybe_unused]] static ECS_CONCAT(Type, _component_registrar)               \
      ECS_CONCAT(Type, _component_registrar_instance){};                       \
  }

inline json save_world(World &world) {
  json out;
  out["scene_name"] = world.currentScene.sceneName;
  out["entities"] = json::array();

  for (EntityId id : world.entities()) {
    json entity_json;
    entity_json["id"] = id;
    entity_json["components"] = json::object();

    for (auto &[name, entry] : ComponentRegistry::instance().entries()) {
      json component_json;
      if (entry.save(world, id, component_json)) {
        entity_json["components"][name] = component_json;
      }
    }
    out["entities"].push_back(entity_json);
  }
  return out;
}

inline std::unordered_map<EntityId, EntityId> load_world(World &world,
                                                         const json &in) {
  std::unordered_map<EntityId, EntityId> remap;
  auto &registry = ComponentRegistry::instance().entries();

  for (auto &entity_json : in.at("entities")) {
    EntityId old_id = entity_json.at("id").get<EntityId>();
    EntityId new_id = world.create_entity();
    remap[old_id] = new_id;

    for (auto &[name, component_json] : entity_json.at("components").items()) {
      auto it = registry.find(name);
      if (it == registry.end()) {
        // Unknown component name
        continue;
      }
      it->second.load(world, new_id, component_json);
    }
  }
  return remap;
}
