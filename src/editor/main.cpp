#include "application.h"
#include "assets/asset_manager.h"
#include "assets/model_loader.h"
#include "ecs/components.h"
#include "ecs/input_manager.h"
#include "ecs/light.h"
#include "ecs/query.h"
#include "ecs/world.h"
#include "rendering/camera.h"
#include "rendering/vulkan_render_context.h"
#include "ui/editor.h"

#include "imgui.h"
#include "ui/editor.h"
#include <GLFW/glfw3.h>
#include <cstdlib>
#include <exception>
#include <glm/ext/vector_float3.hpp>
#include <memory>
#include <print>

void editorStartup(World &world) {
  auto &renderer = *world.get_resource<VulkanRenderingContext *>();
  auto &assetManager = *world.get_resource<AssetManager>();

  auto *models =
      dynamic_cast<ModelLoader *>(assetManager.getLoader<MeshAsset>());
  auto meshEntity = world.create_entity();
  world.add_component(
      meshEntity,
      MeshComponent{.mesh = std::make_shared<Mesh>(
                        models->duplicateMesh(models->getAsset("mesh").mesh))});
  world.add_component(meshEntity, TransformComponent{});

  EditorCamera editorCam;
  editorCam.cam = Camera{};
  editorCam.transform.position = {0.0f, 0.0f, -3.0f};
  editorCam.transform.rotation = {-90.0f, 0.0f, 0.0f};
  editorCam.cam.updateCameraVectors(editorCam.transform);
  world.add_resource<EditorCamera>(std::move(editorCam));

  auto lightEntity = world.create_entity();
  world.add_component(lightEntity, NameComponent{.name = "Light"});
  world.add_component(lightEntity,
                      LightComponent{.lightType = Directional{},
                                     .intensity = 1.0f,
                                     .color = glm::vec3(1.0, 0.0, 0.0),
                                     .isEmitting = true});
  world.add_component(lightEntity,
                      TransformComponent{.position = {5.0f, 1.0f, 5.0f}});

  auto debugIndicatorEntity = world.create_entity();
  world.add_component(debugIndicatorEntity, NameComponent{.name = "Debug Indicator"});
  auto indicatorMesh = renderer->createIndicatorMesh();
  world.add_component(debugIndicatorEntity,
                      MeshComponent{.mesh = indicatorMesh});
  world.add_component(debugIndicatorEntity, TransformComponent{});

  world.add_resource<bool>(false);
  world.add_resource<EditorStatus>();
}

void debugUI(World &world) {
  ImGui::Begin("Debug", nullptr);
  ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

  if (auto *editorCam = world.get_resource<EditorCamera>()) {
    glm::vec3 position = editorCam->cam.getPosition(editorCam->transform);
    ImGui::Text("Position: %.2f, %.2f, %.2f", position.x, position.y,
                position.z);
  }

  uint32_t vertexCount = 0;
  Query<MeshComponent> meshQuery(world);
  meshQuery.for_each([&](EntityId id, MeshComponent &mc) {
    vertexCount += mc.mesh ? mc.mesh->vertexCount : 0;
  });

  ImGui::Text("Vertex Count: %i", vertexCount);
  ImGui::Text("Entity Count: %i", world.entityCount());
  ImGui::End();
}

void lightDebugSync(World &world) {
  auto &lightStorage = world.get_storage<LightComponent>();
  auto &nameStorage = world.get_storage<NameComponent>();
  auto &tcStorage = world.get_storage<TransformComponent>();
  auto &mcStorage = world.get_storage<MeshComponent>();

  EntityId lightId = 0;
  EntityId indicatorId = 0;

  for (std::size_t i = 0; i < nameStorage.size(); ++i) {
    EntityId id = nameStorage.entity_at(i);
    auto *name = nameStorage.get(id);
    if (name && name->name == "Light")
      lightId = id;
    if (name && name->name == "Debug Indicator")
      indicatorId = id;
  }

  if (lightId && indicatorId) {
    auto *lightTc = tcStorage.get(lightId);
    auto *lightLc = lightStorage.get(lightId);
    auto *indTc = tcStorage.get(indicatorId);
    auto *indMc = mcStorage.get(indicatorId);
    if (lightTc && lightLc && indTc && indMc) {
      indTc->position = lightTc->position;
      indTc->rotation = lightTc->rotation;
      indTc->scale = glm::vec3(0.15f);
      indMc->overrideColor = glm::vec4(lightLc->color, 1.0f);
    }
  }
}
UPDATE_SYSTEM(lightDebugSync);

void editorCameraUpdate(World &world) {
  auto *input = world.get_resource<InputManager>();
  auto *window = world.get_resource<GLFWwindow *>();
  auto *dt = world.get_resource<DeltaTime>();
  auto *status = world.get_resource<EditorStatus>();
  if (!input || !window || !dt)
    return;

  static bool captured = false;
  bool wantCapture = status && status->isViewportHovered &&
                     input->isKeybindActive("camera_hold");

  if (wantCapture != captured) {
    captured = wantCapture;
    glfwSetInputMode(*window, GLFW_CURSOR,
                     captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
  }

  auto *editorCam = world.get_resource<EditorCamera>();
  if (!editorCam)
    return;

  if (captured)
    editorCam->cam.processMouseMovement(editorCam->transform,
                                        input->mouseDeltaX, input->mouseDeltaY);

  if (!captured)
    return;

  glm::vec3 direction =
      input->inputVec3("move_right", "move_left", "move_up", "move_down",
                       "move_forward", "move_backward");

  if (direction != glm::vec3(0.0f))
    editorCam->cam.processKeyboard(editorCam->transform, direction, dt->value);
}
UPDATE_SYSTEM(editorCameraUpdate);

int main() {
  try {
    Application app;
    app.setEditorMode(true);
    app.add_startup_system(editorStartup);
    app.addUI([&world = app.getWorld()]() { debugUI(world); }, UIMode::Both);
    app.run();
  } catch (const std::exception &e) {
    std::print("Err: {}\n", e.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
