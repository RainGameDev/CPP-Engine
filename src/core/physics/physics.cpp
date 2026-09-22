#include "ecs/components.h"
#include "ecs/query.h"
#include "ecs/system_registry.h"
#include "ecs/world.h"
#include "physics/physics_components.h"

#include <algorithm>
#include <cstdio>
#include <thread>

namespace {
constexpr JPH::uint kMaxBodies = 65536;
constexpr JPH::uint kNumBodyMutexes = 0;
constexpr JPH::uint kMaxBodyPairs = 65536;
constexpr JPH::uint kMaxContactConstraints = 10240;
constexpr size_t kTempAllocatorSize = 10 * 1024 * 1024;

int physics_thread_count() {
  int n = static_cast<int>(std::thread::hardware_concurrency());
  return n > 1 ? n - 1 : 1;
}
} // namespace

PhysicsWorld::PhysicsWorld()
    : jobSystem(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
                physics_thread_count()),
      tempAllocator(kTempAllocatorSize) {
  JPH::Factory::sInstance = new JPH::Factory();
  JPH::RegisterTypes();
  system.Init(kMaxBodies, kNumBodyMutexes, kMaxBodyPairs,
              kMaxContactConstraints, broadPhaseLayerInterface,
              objectVsBroadPhaseLayerFilter, objectLayerPairFilter);
}

PhysicsWorld::~PhysicsWorld() {
  JPH::UnregisterTypes();
  delete JPH::Factory::sInstance;
  JPH::Factory::sInstance = nullptr;
}

void physicsUpdate(World &world,
                   Query<RigidyBodyComponent, TransformComponent> rigidBodies) {
  rigidBodies.for_each(
      [&](EntityId id, RigidyBodyComponent &rb, TransformComponent &tc) {
        if (rb.motionType != RigidMotionType::Static) {
        }
        // physics shit
      });
}
FIXED_UPDATE_SYSTEM(physicsUpdate);
