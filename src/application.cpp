#include "application.h"
#include "assets/material_loader.h"
#include "ecs/components.h"
#include "ecs/light.h"
#include "ecs/query.h"
#include "ecs/system_registry.h"
#include "ecs/world.h"
#include "glm/ext/vector_float3.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include <GLFW/glfw3.h>
#include <cstdint>
#include <print>

void Application::run() {
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
  glfwSetWindowUserPointer(window, this);
  glfwSetCursorPosCallback(window, mouse_callback);
  glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  if (glfwRawMouseMotionSupported())
    glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

  firstMouse = true;

  glfwSetWindowUserPointer(window, this);
  glfwSetCursorPosCallback(window, mouse_callback);

  renderer.init(window, world);

  cameraEntity = world.create_entity();
  world.get_storage<NameComponent>().get(cameraEntity)->setName("Camera");
  world.add_component(cameraEntity, Camera{});
  world.add_component(cameraEntity, TransformComponent{
                                       .position = {0.0f, 0.0f, -3.0f},
                                       .rotation = {-90.0f, 0.0f, 0.0f},
                                   });

  lightEntity = world.create_entity();
  world.get_storage<NameComponent>().get(lightEntity)->setName("Light");
  world.add_component(lightEntity,
                      LightComponent{.lightType = Directional{},
                                     .intensity = 1.0f,
                                     .color = glm::vec3(1.0, 0.0, 0.0),
                                     .isEmitting = true});
  world.add_component(lightEntity, TransformComponent{.position = {5.0f, 1.0f, 5.0f}});

  debugIndicatorEntity = world.create_entity();
  world.get_storage<NameComponent>().get(debugIndicatorEntity)->setName("Debug Indicator");
  auto indicatorMesh = renderer.createIndicatorMesh();
  world.add_component(debugIndicatorEntity,
                      MeshComponent{.mesh = std::move(indicatorMesh)});
  world.add_component(debugIndicatorEntity, TransformComponent{});

  mainLoop();
  renderer.cleanup();
  glfwDestroyWindow(window);
  glfwTerminate();
}

void Application::tick(float delta_seconds) {
  fixedAccumulator += delta_seconds;
  while (fixedAccumulator >= fixedTimeStep) {
    schedule.run(Stage::FixedUpdate, world);
    fixedAccumulator -= fixedTimeStep;
  }
  schedule.run(Stage::Update, world);
}

void Application::mainLoop() {
  float lastTime = 0.0f;
  while (!glfwWindowShouldClose(window)) {
    float currentTime = static_cast<float>(glfwGetTime());
    float deltaTime = currentTime - lastTime;
    lastTime = currentTime;

    glfwPollEvents();
    processInput(deltaTime);

    auto *cam = world.get_storage<Camera>().get(cameraEntity);
    auto *camTc = world.get_storage<TransformComponent>().get(cameraEntity);
    cam->updateCameraVectors(*camTc);

    UniformBufferObject ubo{};
    ubo.view = cam->getViewMatrix(*camTc);
    ubo.pos = glm::vec4(cam->getPosition(*camTc), 0.0);
    ubo.proj = cam->getProjectionMatrix(renderer.swapChainExtent.width /
                                        (float)renderer.swapChainExtent.height);
    ubo.proj[1][1] *= -1;

    renderer.updateUniformBuffer(renderer.currentFrame, ubo);

    // Sync debug indicator to light
    auto *lightTc = world.get_storage<TransformComponent>().get(lightEntity);
    auto *lightLc = world.get_storage<LightComponent>().get(lightEntity);
    auto *indTc =
        world.get_storage<TransformComponent>().get(debugIndicatorEntity);
    auto *indMc = world.get_storage<MeshComponent>().get(debugIndicatorEntity);
    if (lightTc && lightLc && indTc && indMc) {
      indTc->position = lightTc->position;
      indTc->rotation = lightTc->rotation;
      indTc->scale = glm::vec3(0.15f);
      indMc->overrideColor = glm::vec4(lightLc->color, 1.0f);
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // run all update functions
    tick(deltaTime);

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin("Debug", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoInputs |
                     ImGuiWindowFlags_NoFocusOnAppearing |
                     ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground |
                     ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    glm::vec3 position = cam->getPosition(*camTc);
    ImGui::Text("Position: %.2f, %.2f, %.2f", position.x, position.y,
                position.z);

    uint32_t vertexCount = 0;

    Query<MeshComponent> meshQuery(world);
    meshQuery.for_each([&](EntityId id, MeshComponent &mc) {
      vertexCount += mc.mesh.vertexCount;
    });

    ImGui::Text("Vertex Count: %i", vertexCount);
    ImGui::Text("Entity Count: %i", world.entityCount());

    ImGui::End();

    renderer.framebufferResized = framebufferResized;
    framebufferResized = false;

    ImGui::Render();

    renderer.drawFrame();
  }
}

void Application::cleanup() {}

void Application::processInput(float deltaTime) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
    mouseCaptured = !mouseCaptured;
    if (mouseCaptured) {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      if (glfwRawMouseMotionSupported())
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
      firstMouse = true;
    } else {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      if (glfwRawMouseMotionSupported())
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
    }
    while (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
      glfwPollEvents();
  }

  if (!mouseCaptured)
    return;

  glm::vec3 inputDir = glm::vec3(0.0);

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    inputDir.z += 1.0;
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    inputDir.z -= 1.0;

  if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
    inputDir.y += 1.0;
  if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
    inputDir.y -= 1.0;

  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    inputDir.x -= 1.0;
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    inputDir.x += 1.0;

  auto *cam = world.get_storage<Camera>().get(cameraEntity);
  auto *camTc = world.get_storage<TransformComponent>().get(cameraEntity);
  cam->processKeyboard(*camTc, inputDir, deltaTime);
}

double Application::lastX = 0.0;
double Application::lastY = 0.0;
bool Application::firstMouse = true;

void Application::mouse_callback(GLFWwindow *window, double xpos, double ypos) {
  auto *app = static_cast<Application *>(glfwGetWindowUserPointer(window));

  if (!app->mouseCaptured)
    return;

  if (firstMouse) {
    lastX = xpos;
    lastY = ypos;
    firstMouse = false;
  }

  double deltaX = xpos - lastX;
  double deltaY = lastY - ypos;
  lastX = xpos;
  lastY = ypos;

  auto *cam = app->world.get_storage<Camera>().get(app->cameraEntity);
  auto *camTc =
      app->world.get_storage<TransformComponent>().get(app->cameraEntity);
  cam->processMouseMovement(*camTc, deltaX, deltaY);
}
