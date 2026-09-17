
#pragma once

#include "ecs/component_registry.h"
#include "ecs/entity.h"
#include "ecs/system_registry.h"
#include "ecs/world.h"
#include <algorithm>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

using json = nlohmann::json;

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

/// Replaces the current scene contents with a previously saved snapshot,
/// preserving entity IDs. Mesh handles are serialized as asset names and
/// resolved through the AssetManager after loading.
inline void restore_world(World &world, const json &in) {
  for (EntityId id : world.entities())
    for (auto &[_, storage] : world.currentScene.storages)
      storage->remove(id);

  world.currentScene.sceneName = in.value("scene_name", "Scene");
  auto &registry = ComponentRegistry::instance().entries();
  EntityId maxId = 0;

  AssetManager *assetManager = world.get_resource<AssetManager>();
  for (auto &entity_json : in.at("entities")) {
    EntityId id = entity_json.at("id").get<EntityId>();
    maxId = std::max(maxId, id);
    for (auto &[name, component_json] : entity_json.at("components").items()) {
      auto it = registry.find(name);
      if (it == registry.end())
        continue;
      it->second.load(world, id, component_json);
    }
    if (auto *mc = world.get_storage<MeshComponent>().get(id)) {
      if (assetManager) {
        resolve(*assetManager, mc->mesh);
        resolve(*assetManager, mc->overrideMaterial);
      }
    }
  }
  world.currentScene.nextID = maxId + 1;
}
