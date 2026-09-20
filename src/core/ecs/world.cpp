#include "world.h"
#include "ecs/component_registry.h"
#include <numeric>

EntityId World::create_entity() { return currentScene.nextID++; }

EntityId World::duplicate_entity(EntityId src) {
  if (src == NONE)
    return NONE;
  EntityId dst = create_entity();
  for (auto &[_, entry] : ComponentRegistry::instance().entries()) {
    if (entry.contains(*this, src))
      entry.copy(*this, src, dst);
  }
  return dst;
}

std::vector<EntityId> World::entities() {
  std::vector<EntityId> ids(currentScene.nextID);
  std::iota(ids.begin(), ids.end(), 0);
  return ids;
}
