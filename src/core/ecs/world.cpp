#include "world.h"
#include <numeric>

EntityId World::create_entity() { return currentScene.nextID++; }

std::vector<EntityId> World::entities() {
  std::vector<EntityId> ids(currentScene.nextID);
  std::iota(ids.begin(), ids.end(), 0);
  return ids;
}
