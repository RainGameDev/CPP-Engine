#pragma once
#include "world.h"
#include <functional>

template <typename T>
struct FunctionTraits : FunctionTraits<decltype(&T::operator())> {};

// Plain system void(World&, Query<A,B>)
template <typename Ret, typename... Args>
struct FunctionTraits<Ret (*)(Args...)> {
  using args_tuple = std::tuple<Args...>;
};

// Nonmutable system void(World&, Query<A,B>) const
template <typename Ret, typename ClassT, typename... Args>
struct FunctionTraits<Ret (ClassT::*)(Args...) const> {
  using args_tuple = std::tuple<Args...>;
};

// Mutable system void(World&, Query<A,B>)
template <typename Ret, typename ClassT, typename... Args>
struct FunctionTraits<Ret (ClassT::*)(Args...)> {
  using args_tuple = std::tuple<Args...>;
};

/// Wrapper for the world/queries
using SystemFn = std::function<void(World &)>;

template <typename F> SystemFn make_system(F system) {
  using Args = typename FunctionTraits<std::decay_t<F>>::args_tuple;

  if constexpr (std::tuple_size_v<Args> == 1) {
    return [system = std::move(system)](World &world) { system(world); };
  } else {
    using QueryArg = std::tuple_element_t<1, Args>;
    return [system = std::move(system)](World &world) {
      system(world, QueryArg(world));
    };
  }
}
