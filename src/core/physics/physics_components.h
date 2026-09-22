#pragma once
// clang-format off
#include "ecs/component.h"
#include "ecs/component_registry.h"
#include "ecs/json_glm.h"
#include "glm/ext/vector_float2.hpp"
#include "glm/ext/vector_float3.hpp"
#include "imgui.h"
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <variant>
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

// Registers Jolt default allocator.
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
  std::unordered_map<EntityId, JPH::BodyID> bodies;
};

struct Cube {
  glm::vec3 size{1.0f, 1.0f, 1.0f};
  bool operator==(const Cube &) const = default;

  void inspect() {}

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(Cube, size)
};
struct Plane {
  glm::vec2 size{10.0f, 10.0f};
  bool operator==(const Plane &) const = default;
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(Plane, size)
};
struct Sphere {
  float radius{0.5f};
  bool operator==(const Sphere &) const = default;
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(Sphere, radius)
};
struct Capsule {
  float width{0.5f};
  float height{1.0f};
  bool operator==(const Capsule &) const = default;
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(Capsule, width, height)
};
struct Cylinder {
  float radius{0.5f};
  float height{1.0f};
  bool operator==(const Cylinder &) const = default;
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(Cylinder, radius, height)
};

using ColliderShape = std::variant<Cube, Plane, Sphere, Capsule, Cylinder>;

// {0, size} cube, {1, size} plane, {2, radius} sphere,
// {3, width, height} capsule, {4, radius, height} cylinder.
inline void to_json(nlohmann::json &j, const ColliderShape &s) {
  if (std::holds_alternative<Cube>(s)) {
    j = {0, std::get<Cube>(s).size};
  } else if (std::holds_alternative<Plane>(s)) {
    j = {1, std::get<Plane>(s).size};
  } else if (std::holds_alternative<Sphere>(s)) {
    j = {2, std::get<Sphere>(s).radius};
  } else if (std::holds_alternative<Capsule>(s)) {
    const auto &c = std::get<Capsule>(s);
    j = {3, c.width, c.height};
  } else {
    const auto &c = std::get<Cylinder>(s);
    j = {4, c.radius, c.height};
  }
}
inline void from_json(const nlohmann::json &j, ColliderShape &s) {
  int type = j.is_array() ? j.at(0).get<int>() : j.get<int>();
  switch (type) {
  case 1:
    s = j.is_array() ? Plane{j.at(1).get<glm::vec2>()} : Plane{};
    break;
  case 2:
    s = j.is_array() ? Sphere{j.at(1).get<float>()} : Sphere{};
    break;
  case 3:
    s = j.is_array() ? Capsule{j.at(1).get<float>(), j.at(2).get<float>()}
                     : Capsule{};
    break;
  case 4:
    s = j.is_array() ? Cylinder{j.at(1).get<float>(), j.at(2).get<float>()}
                     : Cylinder{};
    break;
  case 0:
  default:
    s = j.is_array() ? Cube{j.at(1).get<glm::vec3>()} : Cube{};
    break;
  }
}

// Builds the Jolt shape for a collider. JPH::ShapeRefC
make_collider_shape(const ColliderShape &shape);

struct ColliderComponent : Component<ColliderComponent> {
public:
  ColliderShape colliderShape{Cube{}};
  glm::vec3 offset{0.0f, 0.0f, 0.0f};

  bool isEnabled = true;

  void inspect(World &, EntityId) {
    const char *types[] = {"Cube", "Plane", "Sphere", "Capsule", "Cylinder"};
    int currentType = colliderShape.index();
    if (ImGui::Combo("Shape", &currentType, types, 5)) {
      if (currentType == 0)
        colliderShape = Cube{glm::vec3{1.0, 1.0, 1.0}};
      else if (currentType == 1)
        colliderShape = Plane(glm::vec2{1.0, 1.0});
      else if (currentType == 2)
        colliderShape = Sphere{1.0};
      else if (currentType == 3)
        colliderShape = Capsule{1.0, 2.0};
      else if (currentType == 4)
        colliderShape = Cylinder{1.0, 2.0};
    }

    // Cube settings
    if (std::holds_alternative<Cube>(colliderShape)) {
      ImGui::DragFloat3("Size", (float *)&std::get<Cube>(colliderShape).size,
                        0.1f, 0.0f, 100000.0f);
    }
    // Plane settings
    else if (std::holds_alternative<Plane>(colliderShape)) {
      ImGui::DragFloat2("Size", (float *)&std::get<Plane>(colliderShape).size,
                        0.1f, 0.0f, 100000.0f);

    }
    // Sphere settings
    else if (std::holds_alternative<Sphere>(colliderShape)) {
      ImGui::DragFloat("Radius",
                       (float *)&std::get<Sphere>(colliderShape).radius, 0.1f,
                       0.0f, 100000.0f);
    }
    // Capsule settings
    else if (std::holds_alternative<Capsule>(colliderShape)) {
      ImGui::DragFloat("Height",
                       (float *)&std::get<Capsule>(colliderShape).height, 0.1f,
                       0.0f, 100000.0f);
      ImGui::DragFloat("Width",
                       (float *)&std::get<Capsule>(colliderShape).width, 0.1f,
                       0.0f, 100000.0f);
    }
    // Cylinder settings
    else if (std::holds_alternative<Cylinder>(colliderShape)) {
      ImGui::DragFloat("Height",
                       (float *)&std::get<Cylinder>(colliderShape).height, 0.1f,
                       0.0f, 100000.0f);
      ImGui::DragFloat("Radius",
                       (float *)&std::get<Cylinder>(colliderShape).radius, 0.1f,
                       0.0f, 100000.0f);
    }

    ImGui::DragFloat3("offset", (float *)&offset, 0.1f, 0.0f, 100.0f);
    ImGui::Checkbox("Enabled", &isEnabled);
  }
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(ColliderComponent, colliderShape, offset,
                                 isEnabled)
};

REGISTER_COMPONENT(ColliderComponent);

enum class RigidMotionType : int { Static = 0, Kinematic = 1, Dynamic = 2 };

NLOHMANN_JSON_SERIALIZE_ENUM(RigidMotionType, {{RigidMotionType::Static, 0},
                                               {RigidMotionType::Kinematic, 1},
                                               {RigidMotionType::Dynamic, 2}});

struct RigidyBodyComponent : Component<RigidyBodyComponent> {
public:
  RigidMotionType motionType{RigidMotionType::Dynamic};
  float friction{0.2f};
  float weight = 1.0;
  float gravity = -9.81;
  float linearDamping{0.05f};
  float angularDamping{0.05f};
  bool isSensor{false};

  void inspect(World &, EntityId) {
    const char *motionTypes[] = {"Static", "Kinematic", "Dynamic"};
    int current = static_cast<int>(motionType);
    if (ImGui::Combo("Motion Type", &current, motionTypes,
                     IM_ARRAYSIZE(motionTypes))) {
      motionType = static_cast<RigidMotionType>(current);
    }
    ImGui::Checkbox("Sensor", &isSensor);

    ImGui::DragFloat("Weight", &weight, 0.1f,
                     -std::numeric_limits<float>::infinity(),
                     std::numeric_limits<float>::infinity());

    if (motionType != RigidMotionType::Static) {
      ImGui::DragFloat("Gravity", &gravity, 0.1f,
                       -std::numeric_limits<float>::infinity(),
                       std::numeric_limits<float>::infinity());
      ImGui::DragFloat("Linear Damping", &linearDamping, 0.1f,
                       -std::numeric_limits<float>::infinity(),
                       std::numeric_limits<float>::infinity());
      ImGui::DragFloat("Angular Damping", &angularDamping, 0.1f,
                       -std::numeric_limits<float>::infinity(),
                       std::numeric_limits<float>::infinity());
    }
  }
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(RigidyBodyComponent, motionType, friction,
                                 weight, gravity, linearDamping, angularDamping,
                                 isSensor)
};
REGISTER_COMPONENT(RigidyBodyComponent);
