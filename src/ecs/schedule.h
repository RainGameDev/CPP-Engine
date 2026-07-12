
#pragma once
#include <unordered_map>
#include <utility>
#include <vector>

#include "systems.h"
#include "world.h"

enum class Stage { Start, Update, FixedUpdate };

class Schedule {
public:
  template <typename F> void add_system(Stage stage, F system) {
    systems[stage].push_back(make_system(std::move(system)));
  }

  void run(Stage stage, World &world) {
    auto it = systems.find(stage);
    if (it == systems.end())
      return;
    for (auto &sys : it->second)
      sys(world);
  }

private:
  std::unordered_map<Stage, std::vector<SystemFn>> systems;
};
