#pragma once

#include "entity.h"
#include "imgui.h"
#include <algorithm>
#include <concepts>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

class World;

std::unordered_map<std::type_index, std::string> &componentTypeNames();

struct ComponentTag {};

template <typename Derived> struct Component : ComponentTag {
  static constexpr bool showInspectorHeader() { return true; }
};

template <typename T>
concept ComponentType =
    std::derived_from<T, ComponentTag> && requires(T &t, World &w, EntityId id) {
      { t.inspect(w, id) };
    };

struct IComponentStorage {
  virtual ~IComponentStorage() = default;
  virtual bool contains(EntityId id) const = 0;
  virtual void remove(EntityId id) = 0;
  virtual void inspect(World &world, EntityId id) = 0;
};

template <ComponentType T> class ComponentStorage : public IComponentStorage {
public:
  /// Insert Component T for entity ID
  T &insert(EntityId id, T value) {
    if (id >= sparse.size())
      sparse.resize(id + 1, NONE);

    std::uint32_t idx = sparse[id];
    if (idx != NONE) {
      dense[idx] = std::move(value);
      return dense[idx];
    }

    sparse[id] = static_cast<std::uint32_t>(dense.size());
    denseEntities.push_back(id);
    dense.push_back(std::move(value));
    return dense.back();
  }

  //// Get component T for entity ID
  T *get(EntityId id) {
    if (id >= sparse.size() || sparse[id] == NONE)
      return nullptr;
    return &dense[sparse[id]];
  }

  bool contains(EntityId id) const override {
    return id < sparse.size() && sparse[id] != NONE;
  }

  void remove(EntityId id) override {
    if (!contains(id))
      return;
    std::uint32_t idx = sparse[id];
    std::uint32_t last = static_cast<std::uint32_t>(dense.size() - 1);

    dense[idx] = std::move(dense[last]);
    denseEntities[idx] = denseEntities[last];
    sparse[denseEntities[idx]] = idx;

    dense.pop_back();
    denseEntities.pop_back();
    sparse[id] = NONE;
  }

  void inspect(World &world, EntityId id) override {
    if (T *comp = get(id)) {
      if constexpr (T::showInspectorHeader()) {
        auto &names = componentTypeNames();
        auto it = names.find(std::type_index(typeid(T)));
        if (it != names.end())
          ImGui::SeparatorText(it->second.c_str());
      }
      comp->inspect(world, id);
    }
  }

  std::size_t size() const { return dense.size(); }
  EntityId entity_at(std::size_t i) const { return denseEntities[i]; }
  T &component_at(std::size_t i) { return dense[i]; }

private:
  std::vector<std::uint32_t> sparse;
  std::vector<T> dense;
  std::vector<EntityId> denseEntities;
};
