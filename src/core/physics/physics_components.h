#pragma once
// clang-format off
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
// clang-format on

// Object layers what kind of body this is determines what it collides with.
namespace phys_layers {
inline constexpr JPH::ObjectLayer NON_MOVING = 0;
inline constexpr JPH::ObjectLayer MOVING = 1;
inline constexpr JPH::ObjectLayer NUM_LAYERS = 2;
} // namespace phys_layers

// Determines which pairs of object layers collide.
class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
public:
  bool ShouldCollide(JPH::ObjectLayer inObject1,
                     JPH::ObjectLayer inObject2) const override {
    if (inObject1 == phys_layers::NON_MOVING)
      return inObject2 == phys_layers::MOVING;
    return true;
  }
};

// Broadphase layers one boundingvolume tree each static and moving bodies
// get separate trees so the static tree never needs updating.
namespace phys_broadphase {
inline const JPH::BroadPhaseLayer NON_MOVING(0);
inline const JPH::BroadPhaseLayer MOVING(1);
inline constexpr JPH::uint NUM_LAYERS = 2;
} // namespace phys_broadphase

// Maps object layers to broadphase layers.
class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
  BPLayerInterfaceImpl() {
    mObjectToBroadPhase[phys_layers::NON_MOVING] = phys_broadphase::NON_MOVING;
    mObjectToBroadPhase[phys_layers::MOVING] = phys_broadphase::MOVING;
  }

  JPH::uint GetNumBroadPhaseLayers() const override {
    return phys_broadphase::NUM_LAYERS;
  }

  JPH::BroadPhaseLayer
  GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
    return mObjectToBroadPhase[inLayer];
  }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
  const char *
  GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
    if (inLayer == phys_broadphase::NON_MOVING)
      return "NON_MOVING";
    return "MOVING";
  }
#endif

private:
  JPH::BroadPhaseLayer mObjectToBroadPhase[phys_layers::NUM_LAYERS];
};

// Determines whether an object layer collides with a broadphase layer.
class ObjectVsBroadPhaseLayerFilterImpl
    : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
  bool ShouldCollide(JPH::ObjectLayer inLayer1,
                     JPH::BroadPhaseLayer inLayer2) const override {
    if (inLayer1 == phys_layers::NON_MOVING)
      return inLayer2 == phys_broadphase::MOVING;
    return true;
  }
};

// Registers Jolt default allocator. Jolt allocates inside member
// constructors (e.g. JobSystemThreadPool), which run before the PhysicsWorld
// constructor body, so this must be constructed first.
struct JoltAllocatorScope {
  JoltAllocatorScope() { JPH::RegisterDefaultAllocator(); }
};

// World resource owning the Jolt physics system and everything it borrows.
struct PhysicsWorld {
  PhysicsWorld(const PhysicsWorld &) = delete;
  PhysicsWorld &operator=(const PhysicsWorld &) = delete;
  PhysicsWorld();
  ~PhysicsWorld();

  JoltAllocatorScope allocatorScope;

  ObjectLayerPairFilterImpl objectLayerPairFilter;
  BPLayerInterfaceImpl broadPhaseLayerInterface;
  ObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhaseLayerFilter;
  JPH::JobSystemThreadPool jobSystem;
  JPH::TempAllocatorImpl tempAllocator;
  JPH::PhysicsSystem system;
};
