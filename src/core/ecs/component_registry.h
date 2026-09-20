#pragma once

#include "component.h"
#include "entity.h"
#include <functional>
#include <nlohmann/json.hpp>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

#include "world.h"

inline std::unordered_map<std::type_index, std::string> &componentTypeNames() {
  static std::unordered_map<std::type_index, std::string> names;
  return names;
}

class ComponentRegistry {
public:
  struct Entry {
    std::function<bool(World &, EntityId)> contains;
    std::function<void(World &, EntityId)> add_component;
    std::function<void(World &, EntityId)> remove_component;
    std::function<bool(World &, EntityId, nlohmann::json &)> save;
    std::function<void(World &, EntityId, const nlohmann::json &)> load;
    std::function<void(World &, EntityId, EntityId)> copy;
  };

  static ComponentRegistry &instance() {
    static ComponentRegistry inst;
    return inst;
  }

  template <ComponentType T> void register_component(std::string name) {
    componentTypeNames()[std::type_index(typeid(T))] = name;
    Entry entry;
    entry.contains = [](World &world, EntityId id) {
      return world.get_storage<T>().contains(id);
    };

    entry.add_component = [](World &world, EntityId id) {
      world.add_component<T>(id, T{});
    };

    entry.remove_component = [](World &world, EntityId id) {
      world.remove_component<T>(id);
    };

    entry.save = [](World &world, EntityId id, nlohmann::json &out) -> bool {
      auto *c = world.get_storage<T>().get(id);
      if (!c)
        return false;
      out = *c;
      return true;
    };

    entry.copy = [](World &w, EntityId src, EntityId dst) {
      if (auto *c = w.get_storage<T>().get(src))
        w.add_component<T>(dst, *c);
    };
    nlohmann::json defaultJson = nlohmann::json(T{});
    entry.load = [defaultJson](World &world, EntityId id,
                               const nlohmann::json &in) {
      nlohmann::json merged = defaultJson;
      merged.merge_patch(in);
      world.add_component<T>(id, merged.get<T>());
    };
    entries_[name] = std::move(entry);
  }

  const std::unordered_map<std::string, Entry> &entries() const {
    return entries_;
  }

private:
  std::unordered_map<std::string, Entry> entries_;
};

#define ECS_CONCAT_INNER(a, b) a##b
#define ECS_CONCAT(a, b) ECS_CONCAT_INNER(a, b)

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
