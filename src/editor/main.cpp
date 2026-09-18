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
#include "ui/imgui_vulkan.h"
#include <GLFW/glfw3.h>
#include <cstdlib>
#include <exception>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <print>

void editorStartup(World &world) {
  base_style = ImGui::GetStyle();

  EditorCamera editorCam;
  editorCam.cam = Camera{};
  editorCam.transform.position = {0.0f, 0.0f, -3.0f};
  editorCam.transform.rotation =
      glm::angleAxis(glm::radians(-89.0f), glm::vec3(1.0f, 0.0f, 0.0f));
  editorCam.cam.updateCameraVectors(editorCam.transform);
  world.add_resource<EditorCamera>(std::move(editorCam));

  world.add_resource<bool>(false);
  world.add_resource<EditorStatus>();
}

void debugUI(World &world) {
  ImGui::Begin("Debug", nullptr);

  ImGuiIO &io = ImGui::GetIO();
  float fps = io.Framerate;
  float frameTimeMs = io.DeltaTime * 1000.0f;
  float avgFrameTimeMs = 1000.0f / io.Framerate;

  ImGui::Text("FPS: %.1f (%.1f ms)", fps, frameTimeMs);

  if (auto *editorCam = world.get_resource<EditorCamera>()) {
    glm::vec3 position = editorCam->cam.getPosition(editorCam->transform);
    ImGui::Text("Position: %.2f, %.2f, %.2f", position.x, position.y,
                position.z);
  }

  uint32_t vertexCount = 0;
  Query<MeshComponent> meshQuery(world);
  meshQuery.for_each([&](EntityId id, MeshComponent &mc) {
    vertexCount += mc.mesh ? mc.mesh->mesh.vertexCount : 0;
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

  EntityId lightId = NONE;
  EntityId indicatorId = NONE;

  for (std::size_t i = 0; i < nameStorage.size(); ++i) {
    EntityId id = nameStorage.entity_at(i);
    auto *name = nameStorage.get(id);
    if (name && name->name == "Light")
      lightId = id;
    if (name && name->name == "Debug Indicator")
      indicatorId = id;
  }

  if (lightId != NONE && indicatorId != NONE) {
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
  app.addPreTickUI([&world = app.getWorld()]() { topbar(world); });
  app.addUI([&world = app.getWorld()]() { debugUI(world); }, UIMode::Both);

    app.run();
  } catch (const std::exception &e) {
    std::print("Err: {}\n", e.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
