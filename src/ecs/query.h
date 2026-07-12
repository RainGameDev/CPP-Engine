#pragma once

#include <functional>

#include "world.h"

template <typename... Components> class Query {
public:
  explicit Query(World &world) : world_(world) {}

  void for_each(std::function<void(EntityId, Components &...)> func) {
    iterate_smallest<Components...>(func);
  }

private:
  template <typename... Ts>
  void iterate_smallest(std::function<void(EntityId, Components &...)> &func) {
    std::size_t sizes[] = {world_.get_storage<Ts>().size()...};
    std::size_t min_size = sizes[0];
    for (auto s : sizes)
      min_size = std::min(min_size, s);

    dispatch_smallest<Ts...>(min_size, func);
  }

  template <typename First, typename... Rest>
  void dispatch_smallest(std::size_t min_size,
                         std::function<void(EntityId, Components &...)> &func) {
    auto &storage = world_.get_storage<First>();
    if (storage.size() != min_size) {
      if constexpr (sizeof...(Rest) > 0) {
        dispatch_smallest<Rest...>(min_size, func);
        return;
      }
    }
    for (std::size_t i = 0; i < storage.size(); ++i) {
      EntityId id = storage.entity_at(i);
      if (auto tup = try_get<Components...>(id)) {
        std::apply([&](auto &...refs) { func(id, refs...); }, *tup);
      }
    }
  }

  template <typename... Ts>
  std::optional<std::tuple<Ts &...>> try_get(EntityId id) {
    auto ptrs = std::make_tuple(world_.get_storage<Ts>().get(id)...);
    bool all_present =
        std::apply([](auto *...p) { return (... && (p != nullptr)); }, ptrs);
    if (!all_present)
      return std::nullopt;
    return std::apply([](auto *...p) { return std::tuple<Ts &...>(*p...); },
                      ptrs);
  }

  World &world_;
};
