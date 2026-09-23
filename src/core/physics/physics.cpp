#include "ecs/components.h"
#include "ecs/query.h"
#include "ecs/system_registry.h"
#include "ecs/world.h"
#include "physics/physics_components.h"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <unordered_set>

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
  // Remove and destroy any bodies still alive (e.g. an unsaved scene).
  auto &bodyInterface = system.GetBodyInterface();
  for (auto &[_, record] : bodies) {
    bodyInterface.RemoveBody(record.bodyID);
    bodyInterface.DestroyBody(record.bodyID);
  }
  bodies.clear();

  JPH::UnregisterTypes();
  delete JPH::Factory::sInstance;
  JPH::Factory::sInstance = nullptr;
}

namespace {
constexpr float kGravity = 9.81f;
/// Matches Application::fixedTimeStep so one collision step is exactly one
/// fixed update.
constexpr float kFixedDt = 1.0f / 60.0f;

JPH::EMotionType to_jolt_motion(RigidMotionType type) {
  switch (type) {
  case RigidMotionType::Static:
    return JPH::EMotionType::Static;
  case RigidMotionType::Kinematic:
    return JPH::EMotionType::Kinematic;
  case RigidMotionType::Dynamic:
    return JPH::EMotionType::Dynamic;
  }
  return JPH::EMotionType::Dynamic;
}

JPH::ObjectLayer to_object_layer(RigidMotionType type) {
  return type == RigidMotionType::Static ? phys_layers::NON_MOVING
                                         : phys_layers::MOVING;
}

glm::quat to_glm(const JPH::Quat &q) {
  return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
}
glm::vec3 to_glm(const JPH::RVec3 &v) {
  return glm::vec3(static_cast<float>(v.GetX()), static_cast<float>(v.GetY()),
                   static_cast<float>(v.GetZ()));
}
JPH::Quat to_jolt(const glm::quat &q) { return JPH::Quat(q.x, q.y, q.z, q.w); }

// World-space anchor for the body: the entity origin plus the collider offset
// rotated by the entity rotation.
JPH::RVec3 body_position(const TransformComponent &tc,
                         const ColliderComponent &collider) {
  glm::vec3 p = tc.position + tc.rotation * collider.offset;
  return JPH::RVec3(static_cast<double>(p.x), static_cast<double>(p.y),
                    static_cast<double>(p.z));
}

// Each step hash the component settings that affect the body.
// When the hash changes the body is rebuilt.
std::size_t mix_hash(std::size_t h, uint64_t v) {
  h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
  return h;
}
std::size_t mix_float(std::size_t h, float f) {
  uint32_t bits;
  std::memcpy(&bits, &f, sizeof(bits));
  return mix_hash(h, bits);
}

std::size_t collider_signature(const ColliderShape &shape) {
  std::size_t h = mix_hash(0, shape.index());
  if (const auto *cube = std::get_if<Cube>(&shape))
    h = mix_float(mix_float(mix_float(h, cube->size.x), cube->size.y),
                  cube->size.z);
  else if (const auto *plane = std::get_if<Plane>(&shape))
    h = mix_float(mix_float(h, plane->size.x), plane->size.y);
  else if (const auto *sphere = std::get_if<Sphere>(&shape))
    h = mix_float(h, sphere->radius);
  else if (const auto *capsule = std::get_if<Capsule>(&shape))
    h = mix_float(mix_float(h, capsule->width), capsule->height);
  else if (const auto *cylinder = std::get_if<Cylinder>(&shape))
    h = mix_float(mix_float(h, cylinder->radius), cylinder->height);
  return h;
}

std::size_t body_signature(const RigidyBodyComponent &rb,
                           const ColliderComponent &collider) {
  std::size_t h = collider_signature(collider.colliderShape);
  h = mix_float(h, collider.offset.x);
  h = mix_float(h, collider.offset.y);
  h = mix_float(h, collider.offset.z);
  h = mix_hash(h, static_cast<uint64_t>(rb.motionType));
  h = mix_float(h, rb.friction);
  h = mix_float(h, rb.restitution);
  h = mix_float(h, rb.weight);
  h = mix_float(h, rb.gravity);
  h = mix_float(h, rb.linearDamping);
  h = mix_float(h, rb.angularDamping);
  h = mix_hash(h, rb.isSensor ? 1 : 0);
  return h;
}

// Destroys the entitys body if it exists.
void remove_body(PhysicsWorld &pw, EntityId id) {
  auto it = pw.bodies.find(id);
  if (it == pw.bodies.end())
    return;
  auto &bodyInterface = pw.system.GetBodyInterface();
  bodyInterface.RemoveBody(it->second.bodyID);
  bodyInterface.DestroyBody(it->second.bodyID);
  pw.bodies.erase(it);
}

// Rebuilds the entitys body from the components, stores it
void sync_body(PhysicsWorld &pw, EntityId id, const RigidyBodyComponent &rb,
               const TransformComponent &tc, const ColliderComponent &collider,
               std::size_t signature) {
  remove_body(pw, id);

  JPH::ShapeRefC shape = make_collider_shape(collider.colliderShape);
  if (shape == nullptr)
    return;

  JPH::EMotionType motion = to_jolt_motion(rb.motionType);
  JPH::BodyCreationSettings settings(shape, body_position(tc, collider),
                                     to_jolt(tc.rotation), motion,
                                     to_object_layer(rb.motionType));
  settings.mFriction = rb.friction;
  settings.mRestitution = rb.restitution;
  settings.mLinearDamping = rb.linearDamping;
  settings.mAngularDamping = rb.angularDamping;
  settings.mGravityFactor =
      rb.gravity == 0.0f ? 0.0f : std::abs(rb.gravity) / kGravity;
  settings.mIsSensor = rb.isSensor;
  if (motion != JPH::EMotionType::Static) {
    settings.mOverrideMassProperties =
        JPH::EOverrideMassProperties::CalculateInertia;
    settings.mMassPropertiesOverride.mMass = std::max(rb.weight, 1e-4f);
  }

  auto &bodyInterface = pw.system.GetBodyInterface();
  JPH::BodyID bodyID =
      bodyInterface.CreateAndAddBody(settings, JPH::EActivation::Activate);
  if (!bodyID.IsInvalid())
    pw.bodies[id] = PhysicsBodyRecord{bodyID, signature};
}
} // namespace

void physicsUpdate(
    World &world,
    Query<RigidyBodyComponent, TransformComponent, ColliderComponent>
        rigidBodies) {
  PhysicsWorld *pw = world.get_resource<PhysicsWorld>();
  if (!pw)
    return;
  auto &bodyInterface = pw->system.GetBodyInterface();

  // Which entities still have the physics component
  std::unordered_set<EntityId> seen;

  // get bodies with entities.
  rigidBodies.for_each([&](EntityId id, RigidyBodyComponent &rb,
                           TransformComponent &tc,
                           ColliderComponent &collider) {
    seen.insert(id);

    if (!collider.isEnabled) {
      remove_body(*pw, id);
      return;
    }

    std::size_t signature = body_signature(rb, collider);
    auto it = pw->bodies.find(id);
    if (it != pw->bodies.end() && it->second.signature == signature) {
      // Body already matches the components
      JPH::EActivation activation = rb.motionType == RigidMotionType::Dynamic
                                        ? JPH::EActivation::DontActivate
                                        : JPH::EActivation::Activate;
      bodyInterface.SetPositionAndRotationWhenChanged(
          it->second.bodyID, body_position(tc, collider), to_jolt(tc.rotation),
          activation);
      return;
    }

    sync_body(*pw, id, rb, tc, collider, signature);
  });

  // Destroy bodies when entity lose theircomponents
  for (auto it = pw->bodies.begin(); it != pw->bodies.end();) {
    if (seen.contains(it->first)) {
      ++it;
      continue;
    }
    const JPH::BodyID bodyID = it->second.bodyID;
    bodyInterface.RemoveBody(bodyID);
    bodyInterface.DestroyBody(bodyID);
    it = pw->bodies.erase(it);
  }

  // step the simulation
  pw->system.Update(kFixedDt, 1, &pw->tempAllocator, &pw->jobSystem);

  // write dynamic body transforms back to the entities.
  rigidBodies.for_each([&](EntityId id, RigidyBodyComponent &rb,
                           TransformComponent &tc,
                           ColliderComponent &collider) {
    if (rb.motionType != RigidMotionType::Dynamic || !collider.isEnabled)
      return;
    auto it = pw->bodies.find(id);
    if (it == pw->bodies.end())
      return;
    JPH::RVec3 pos;
    JPH::Quat rot;
    bodyInterface.GetPositionAndRotation(it->second.bodyID, pos, rot);
    tc.rotation = to_glm(rot);
    tc.position = to_glm(pos) - tc.rotation * collider.offset;
  });
}
FIXED_UPDATE_SYSTEM(physicsUpdate);
