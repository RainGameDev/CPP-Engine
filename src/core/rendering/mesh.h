#pragma once

#include "assets/asset_payload.h"
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
  Handle<MaterialAsset> overrideMaterial;
  glm::vec4 overrideColor{0.0f};

  void inspect(World &world, EntityId) {
    auto *assetManager = world.get_resource<AssetManager>();
    auto *modelLoader =
        assetManager ? assetManager->getLoader<MeshAsset>() : nullptr;

    const float previewSize = 96.0f;

    // load the mesh or its null
    const MeshAsset *previewAsset =
        modelLoader ? modelLoader->tryGetAsset(mesh.assetName) : nullptr;

    if (assetManager) {
      assetDragDropField(
          "Mesh", AssetDragPayload::Mesh, *assetManager, mesh, previewSize,
          [&, this](float size) {
            // if the mesh exists render the preview image for it
            if (previewAsset &&
                previewAsset->previewTexture != VK_NULL_HANDLE) {
              ImGui::Image((ImTextureID)previewAsset->previewTexture,
                           ImVec2(size, size));
            } else {
              // otherwise just draw a box and some text
              ImVec2 p = ImGui::GetCursorScreenPos();
              ImDrawList *drawList = ImGui::GetWindowDrawList();
              drawList->AddRectFilled(p, ImVec2(p.x + size, p.y + size),
                                      IM_COL32(70, 70, 70, 255), 4.0f);
              const char *text = "No mesh \n selected";
              ImVec2 textSize = ImGui::CalcTextSize(text);
              drawList->AddText(ImVec2(p.x + (size - textSize.x) * 0.5f,
                                       p.y + (size - textSize.y) * 0.5f),
                                IM_COL32(200, 200, 200, 255), text);
              ImGui::Dummy(ImVec2(size, size));
            }
          },
          [this]() { overrideColor = glm::vec4(0.0f); });
    }

    ImGui::ColorEdit4("Override Color", &overrideColor.x);

    // Override Material
    ImGui::Separator();

    if (assetManager) {
      assetDragDropField(
          "Material", AssetDragPayload::Material, *assetManager,
          overrideMaterial, previewSize, [this](float size) {
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImDrawList *drawList = ImGui::GetWindowDrawList();
            ImU32 boxColor = overrideMaterial ? IM_COL32(0, 150, 140, 255)
                                              : IM_COL32(70, 70, 70, 255);
            drawList->AddRectFilled(pos, ImVec2(pos.x + size, pos.y + size),
                                    boxColor, 4.0f);
            if (overrideMaterial) {
              drawList->AddText(ImVec2(pos.x + 6.0f, pos.y + 6.0f),
                                IM_COL32(240, 240, 240, 255),
                                overrideMaterial.assetName.c_str());
            } else {
              const char *hint = "Drag material \n here";
              ImVec2 hintSize = ImGui::CalcTextSize(hint);
              drawList->AddText(ImVec2(pos.x + (size - hintSize.x) * 0.5f,
                                       pos.y + (size - hintSize.y) * 0.5f),
                                IM_COL32(200, 200, 200, 255), hint);
            }
            ImGui::Dummy(ImVec2(size, size));
          });
    }
  }
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(MeshComponent, mesh, overrideMaterial,
                                 overrideColor)
};
