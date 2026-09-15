#pragma once

#include "assets/handle.h"
#include "assets/material_loader.h"
#include "ecs/component.h"
#include "ecs/json_glm.h"
#include "ecs/world.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include <cfloat>
#include <cstdint>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

struct Mesh {
  vk::raii::Buffer vertexBuffer{nullptr};
  vk::raii::DeviceMemory vertexMemory{nullptr};
  vk::raii::Buffer indexBuffer{nullptr};
  vk::raii::DeviceMemory indexMemory{nullptr};
  uint32_t vertexCount = 0;
  uint32_t indexCount = 0;
  MaterialAsset *material = nullptr;
};

struct MeshAsset {
  Mesh mesh;

  // Preview thumbnail resources
  vk::raii::Image previewImage{nullptr};
  vk::raii::DeviceMemory previewMemory{nullptr};
  vk::raii::ImageView previewView{nullptr};

  vk::raii::Image previewDepthImage{nullptr};
  vk::raii::DeviceMemory previewDepthMemory{nullptr};
  vk::raii::ImageView previewDepthView{nullptr};

  vk::raii::Sampler previewSampler{nullptr};
  vk::raii::Framebuffer previewFramebuffer{nullptr};

  VkDescriptorSet previewTexture = VK_NULL_HANDLE;

  // Bounding box captured at load time
  glm::vec3 previewMin{FLT_MAX};
  glm::vec3 previewMax{-FLT_MAX};

  // Interactive preview orientation.
  float previewYaw = 0.0f;
  float previewPitch = 0.0f;

  static constexpr uint32_t kPreviewSize = 128;
  static constexpr vk::Format kPreviewColorFormat = vk::Format::eR8G8B8A8Unorm;
  static constexpr vk::Format kPreviewDepthFormat = vk::Format::eD32Sfloat;

  MeshAsset() = default;
  MeshAsset(MeshAsset &&other) noexcept
      : mesh(std::move(other.mesh)),
        previewImage(std::move(other.previewImage)),
        previewMemory(std::move(other.previewMemory)),
        previewView(std::move(other.previewView)),
        previewDepthImage(std::move(other.previewDepthImage)),
        previewDepthMemory(std::move(other.previewDepthMemory)),
        previewDepthView(std::move(other.previewDepthView)),
        previewSampler(std::move(other.previewSampler)),
        previewFramebuffer(std::move(other.previewFramebuffer)),
        previewTexture(other.previewTexture), previewMin(other.previewMin),
        previewMax(other.previewMax), previewYaw(other.previewYaw),
        previewPitch(other.previewPitch) {
    other.previewTexture = VK_NULL_HANDLE;
  }
  MeshAsset &operator=(MeshAsset &&other) noexcept {
    if (this != &other) {
      if (previewTexture != VK_NULL_HANDLE)
        ImGui_ImplVulkan_RemoveTexture(previewTexture);
      mesh = std::move(other.mesh);
      previewImage = std::move(other.previewImage);
      previewMemory = std::move(other.previewMemory);
      previewView = std::move(other.previewView);
      previewDepthImage = std::move(other.previewDepthImage);
      previewDepthMemory = std::move(other.previewDepthMemory);
      previewDepthView = std::move(other.previewDepthView);
      previewSampler = std::move(other.previewSampler);
      previewFramebuffer = std::move(other.previewFramebuffer);
      previewTexture = other.previewTexture;
      previewMin = other.previewMin;
      previewMax = other.previewMax;
      previewYaw = other.previewYaw;
      previewPitch = other.previewPitch;
      other.previewTexture = VK_NULL_HANDLE;
    }
    return *this;
  }
  MeshAsset(const MeshAsset &) = delete;
  MeshAsset &operator=(const MeshAsset &) = delete;

  ~MeshAsset() {
    if (previewTexture != VK_NULL_HANDLE) {
      ImGui_ImplVulkan_RemoveTexture(previewTexture);
      previewTexture = VK_NULL_HANDLE;
    }
  }
};

struct MeshComponent : Component<MeshComponent> {
  Handle<MeshAsset> mesh;
  glm::vec4 overrideColor{0.0f};

  void inspect(World &world, EntityId) {
    auto *assetManager = world.get_resource<AssetManager>();
    auto *modelLoader =
        assetManager ? assetManager->getLoader<MeshAsset>() : nullptr;

    const char *preview =
        mesh.assetName.empty() ? "None" : mesh.assetName.c_str();
    if (ImGui::BeginCombo("Asset", preview)) {
      if (ImGui::Selectable("None", mesh.assetName.empty())) {
        mesh = Handle<MeshAsset>{};
        overrideColor = glm::vec4(0.0f);
      }
      if (modelLoader) {
        for (const auto &[name, _] : modelLoader->getAssets()) {
          if (ImGui::Selectable(name.c_str(), mesh.assetName == name)) {
            mesh.assetName = name;
            resolve(*assetManager, mesh);
          }
        }
      }
      ImGui::EndCombo();
    }

    ImGui::ColorEdit4("Override Color", &overrideColor.x);
  }
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(MeshComponent, mesh, overrideColor)
};
