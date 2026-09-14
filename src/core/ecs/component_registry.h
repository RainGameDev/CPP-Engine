#pragma once

#include "component.h"
#include "entity.h"
#include <functional>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

#include "world.h"

class ComponentRegistry {
public:
  struct Entry {
    // Returns false if this entity doesn't have the component.
    std::function<bool(World &, EntityId, nlohmann::json &)> save;
    std::function<void(World &, EntityId, const nlohmann::json &)> load;
  };

  static ComponentRegistry &instance() {
    static ComponentRegistry inst;
    return inst;
  }

  template <ComponentType T> void register_component(std::string name) {
    Entry entry;
    entry.save = [](World &world, EntityId id,
                    nlohmann::json &out) -> bool {
      auto *c = world.get_storage<T>().get(id);
      if (!c)
        return false;
      out = *c; // calls to_json(json&, const T&)
      return true;
    };
    entry.load = [](World &world, EntityId id, const nlohmann::json &in) {
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