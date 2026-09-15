#pragma once

#include "component.h"
#include "entity.h"
#include "scene.h"
#include <any>
#include <cstdint>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

class World {
public:
  EntityId create_entity();

  /// Adds a resource of type T.
  template <typename T, typename... Args> void add_resource(Args &&...args) {
    if (has_resource<T>()) {
      return;
    }

    auto ptr = std::make_shared<T>(std::forward<Args>(args)...);
    resources[std::type_index(typeid(T))] = ptr;
  }

  /// Does the world have this resource?
  template <typename T> bool has_resource() {
    auto it = resources.find(std::type_index(typeid(T)));
    if (it == resources.end())
      return false;

    return true;
  }

  /// Gets a resource of type T.
  template <typename T> T *get_resource() {
    auto it = resources.find(std::type_index(typeid(T)));
    if (it == resources.end())
      return nullptr;
    return std::any_cast<std::shared_ptr<T>>(it->second).get();
  }

  /// Adds a component of type T to entity ID.
  template <ComponentType T> T &add_component(EntityId id, T component) {
    return get_storage<T>().insert(id, std::move(component));
  }

  /// Removes a component of type T from entity ID.
  template <ComponentType T> void remove_component(EntityId id) {
    return get_storage<T>().remove(id);
  }

  template <ComponentType T> T *get_component(EntityId id) {
    return get_storage<T>().get(id);
  }

  template <ComponentType T> ComponentStorage<T> &get_storage() {
    auto key = std::type_index(typeid(T));
    auto it = currentScene.storages.find(key);
    if (it == currentScene.storages.end()) {
      auto storage = std::make_unique<ComponentStorage<T>>();
      auto *raw = storage.get();
      currentScene.storages.emplace(key, std::move(storage));
      return *raw;
    }
    return *static_cast<ComponentStorage<T> *>(it->second.get());
  }

  uint32_t entityCount() { return currentScene.nextID - 1; }

  std::vector<EntityId> entities();

  void delete_entity(EntityId id) {
    for (auto &[_, storage] : currentScene.storages)
      storage->remove(id);
  }

  void inspect_entity(EntityId id) {
    for (auto &[_, storage] : currentScene.storages) {
      if (storage->contains(id))
        storage->inspect(*this, id);
    }
  }

  Scene currentScene;

private:
  std::unordered_map<std::type_index, std::any> resources;
};
