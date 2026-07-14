

#include "ecs/component.h"
#include "ecs/entity.h"
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
struct Scene {
  std::string sceneName = "Scene";
  EntityId nextID = 0;
  std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>>
      storages;
};
