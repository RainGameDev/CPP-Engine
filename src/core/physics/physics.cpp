#include "ecs/components.h"
#include "ecs/query.h"
#include "ecs/system_registry.h"
#include "ecs/world.h"
#include "physics/physics_components.h"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
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

namespace {
// Smallest dimension to build.
constexpr float kMinShapeExtent = 1e-3f;
constexpr float kPlaneThickness = 1.0f;
} // namespace

JPH::ShapeRefC make_collider_shape(const ColliderShape &shape) {
  if (const auto *cube = std::get_if<Cube>(&shape)) {
    return new JPH::BoxShape(
        JPH::Vec3(std::max(cube->size.x * 0.5f, kMinShapeExtent),
                  std::max(cube->size.y * 0.5f, kMinShapeExtent),
                  std::max(cube->size.z * 0.5f, kMinShapeExtent)));
  }
  if (const auto *plane = std::get_if<Plane>(&shape)) {
    // Jolt planes are stupid doodoo and are infinitem, so jus tmap it to a slab
    return new JPH::BoxShape(JPH::Vec3(
        std::max(plane->size.x * 0.5f, kMinShapeExtent), kPlaneThickness * 0.5f,
        std::max(plane->size.y * 0.5f, kMinShapeExtent)));
  }
  if (const auto *sphere = std::get_if<Sphere>(&shape)) {
    return new JPH::SphereShape(std::max(sphere->radius, kMinShapeExtent));
  }
  if (const auto *capsule = std::get_if<Capsule>(&shape)) {
    // Jolt wants the half height
    float radius = std::max(capsule->width * 0.5f, kMinShapeExtent);
    float halfCyl = std::max(capsule->height * 0.5f - radius, kMinShapeExtent);
    return new JPH::CapsuleShape(halfCyl, radius);
  }
  const auto &cylinder = std::get<Cylinder>(shape);
  return new JPH::CylinderShape(
      std::max(cylinder.height * 0.5f, kMinShapeExtent),
      std::max(cylinder.radius, kMinShapeExtent));
}

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

void physicsUpdate(
    World &world,
    Query<RigidyBodyComponent, TransformComponent, ColliderComponent>
        rigidBodies) {
  rigidBodies.for_each([&](EntityId id, RigidyBodyComponent &rb,
                           TransformComponent &tc,
                           ColliderComponent &collider) {
    if (rb.motionType != RigidMotionType::Static) {
    }
    // physics shit
  });
}
FIXED_UPDATE_SYSTEM(physicsUpdate);
