#include "application.h"
#include "assets/asset_manager.h"
#include "assets/material_loader.h"
#include "assets/model_loader.h"
#include "ecs/components.h"
#include "ecs/input_manager.h"
#include "ecs/world.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include <GLFW/glfw3.h>
#include <cstdint>

void Application::run() {
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
  glfwSetWindowUserPointer(window, this);
  glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  if (glfwRawMouseMotionSupported())
    glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

  glfwSetWindowUserPointer(window, this);

  world.add_resource<AssetManager>();
  AssetManager &assetManager = *world.get_resource<AssetManager>();

  renderer.editorMode = editorMode;
  renderer.init(window, world);
  world.add_resource<VulkanRenderingContext *>(&renderer);

  world.add_resource<InputManager>();
  auto *inputManager = world.get_resource<InputManager>();
  inputManager->window = window;
  inputManager->addKeybind({GLFW_KEY_W, GLFW_REPEAT}, "move_forward");
  inputManager->addKeybind({GLFW_KEY_S, GLFW_REPEAT}, "move_backward");
  inputManager->addKeybind({GLFW_KEY_A, GLFW_REPEAT}, "move_left");
  inputManager->addKeybind({GLFW_KEY_D, GLFW_REPEAT}, "move_right");
  inputManager->addKeybind({GLFW_KEY_SPACE, GLFW_REPEAT}, "move_up");
  inputManager->addKeybind({GLFW_KEY_LEFT_SHIFT, GLFW_REPEAT}, "move_down");
  assetManager.addLoader(std::make_unique<ShaderLoader>(renderer.device));
  assetManager.loadDirectory("assets/shaders");
  assetManager.addLoader(std::make_unique<TextureLoader>(
      renderer.device, renderer.physicalDevice, renderer.commandPool,
      renderer.queue, renderer.queueIndex));
  assetManager.loadDirectory("assets/textures");
  assetManager.addLoader(std::make_unique<MaterialLoader>(
      renderer.device, renderer.physicalDevice, renderer.materialSetLayout,
      renderer.descriptorPool, renderer.commandPool, renderer.queue,
      assetManager));
  assetManager.loadDirectory("assets/materials");
  assetManager.addLoader(std::make_unique<ModelLoader>(
      renderer.device, renderer.physicalDevice, renderer.commandPool,
      renderer.queue, assetManager));
  assetManager.loadDirectory("assets/models");

  schedule.run(Stage::Start, world);

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

    auto &camStorage = world.get_storage<Camera>();
    if (camStorage.size() == 0)
      continue;
    EntityId camId = camStorage.entity_at(0);
    auto *cam = camStorage.get(camId);
    auto *camTc = world.get_storage<TransformComponent>().get(camId);

    UniformBufferObject ubo{};
    ubo.view = cam->getViewMatrix(*camTc);
    ubo.pos = glm::vec4(cam->getPosition(*camTc), 0.0);
    ubo.proj = cam->getProjectionMatrix(renderer.viewportExtent.width /
                                        (float)renderer.viewportExtent.height);
    ubo.proj[1][1] *= -1;

    renderer.updateUniformBuffer(renderer.currentFrame, ubo);

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    for (auto &cb : preTickUIs) {
      if (cb.mode == UIMode::Both ||
          (cb.mode == UIMode::Editor && editorMode) ||
          (cb.mode == UIMode::Game && !editorMode))
        cb.func();
    }

    // run all update functions
    tick(deltaTime);

    renderer.recreateViewportIfNeeded();

    if (!editorMode) {
      renderer.pendingViewportExtent = renderer.swapChainExtent;
    }

    for (auto &cb : postTickUIs) {
      if (cb.mode == UIMode::Both ||
          (cb.mode == UIMode::Editor && editorMode) ||
          (cb.mode == UIMode::Game && !editorMode))
        cb.func();
    }

    ImGui::Render();

    renderer.framebufferResized = framebufferResized;
    framebufferResized = false;

    renderer.drawFrame();
  }
}

void Application::cleanup() {}

void Application::processInput(float deltaTime) {
  auto *input = world.get_resource<InputManager>();
  input->update();

  auto &camStorage = world.get_storage<Camera>();
  if (camStorage.size() == 0)
    return;
  EntityId camId = camStorage.entity_at(0);
  auto *cam = camStorage.get(camId);
  auto *camTc = world.get_storage<TransformComponent>().get(camId);

  if (mouseCaptured && cam && camTc) {
    cam->processMouseMovement(*camTc, input->mouseDeltaX, input->mouseDeltaY);
  }

  glm::vec3 direction =
      input->inputVec3("move_right", "move_left", "move_up", "move_down",
                       "move_forward", "move_backward");

  if (direction != glm::vec3(0.0f) && cam && camTc)
    cam->processKeyboard(*camTc, direction, deltaTime);
}
