#pragma once

#include "component.h"
#include "entity.h"
#include <any>
#include <cstdint>
#include <memory>
#include <typeindex>
#include <unordered_map>

class World {
public:
  EntityId create_entity() { return nextID++; }

  template <typename T, typename... Args> T &add_resource(Args &&...args) {
    auto ptr = std::make_shared<T>(std::forward<Args>(args)...);
    resources[std::type_index(typeid(T))] = ptr;
    return *ptr;
  }

  template <typename T> T *get_resource() {
    auto it = resources.find(std::type_index(typeid(T)));
    if (it == resources.end())
      return nullptr;
    return std::any_cast<std::shared_ptr<T>>(it->second).get();
  }

  template <typename T> T &add_component(EntityId id, T component) {
    return get_storage<T>().insert(id, std::move(component));
  }

  template <typename T> ComponentStorage<T> &get_storage() {
    auto key = std::type_index(typeid(T));
    auto it = storages.find(key);
    if (it == storages.end()) {
      auto storage = std::make_unique<ComponentStorage<T>>();
      auto *raw = storage.get();
      storages.emplace(key, std::move(storage));
      return *raw;
    }
    return *static_cast<ComponentStorage<T> *>(it->second.get());
  }

  uint32_t entityCount() { return nextID - 1; }

private:
  EntityId nextID = 0;
  std::unordered_map<std::type_index, std::any> resources;
  std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>>
      storages;
};
