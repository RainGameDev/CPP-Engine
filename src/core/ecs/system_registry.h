
#pragma once

#include "schedule.h"
#include <functional>
#include <vector>

class SystemRegistry {
public:
  static SystemRegistry &instance() {
    static SystemRegistry inst;
    return inst;
  }

  void register_system(std::function<void(Schedule &)> registrar) {
    registrars.push_back(std::move(registrar));
  }

  void apply_to(Schedule &schedule) {
    for (auto &registrar : registrars)
      registrar(schedule);
  }

private:
  std::vector<std::function<void(Schedule &)>> registrars;
};

#define ECS_CONCAT_INNER(a, b) a##b
#define ECS_CONCAT(a, b) ECS_CONCAT_INNER(a, b)

#define ECS_SYSTEM(stage_expr, func)                                           \
  namespace {                                                                  \
  struct ECS_CONCAT(func, registrar) {                                         \
    ECS_CONCAT(func, registrar)() {                                            \
      SystemRegistry::instance().register_system(                              \
          [](Schedule &schedule) { schedule.add_system(stage_expr, func); });  \
    }                                                                          \
  };                                                                           \
  [[maybe_unused]] static ECS_CONCAT(func, registrar)                          \
      ECS_CONCAT(func, registrar_instance){};                                  \
  }

#define STARTUP_SYSTEM(func) ECS_SYSTEM(Stage::Start, func)
#define UPDATE_SYSTEM(func) ECS_SYSTEM(Stage::Update, func)
#define FIXED_UPDATE_SYSTEM(func) ECS_SYSTEM(Stage::FixedUpdate, func)
