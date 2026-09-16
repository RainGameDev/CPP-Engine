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
  glm::vec4 overrideColor{0.0f};

  void inspect(World &world, EntityId) {
    auto *assetManager = world.get_resource<AssetManager>();
    auto *modelLoader =
        assetManager ? assetManager->getLoader<MeshAsset>() : nullptr;

    const float previewSize = 96.0f;
    const char *label = "Mesh";
    ImVec2 labelSize = ImGui::CalcTextSize(label);
    const float labelOffsetY = (previewSize - labelSize.y) * 0.5f;

    // load the mesh or its null
    const MeshAsset *previewAsset =
        modelLoader ? modelLoader->tryGetAsset(mesh.assetName) : nullptr;

    // vertically center the label against the preview below it
    ImVec2 rowCursor = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(rowCursor.x, rowCursor.y + labelOffsetY));
    ImGui::Text("%s", label);
    ImGui::SetCursorPos(
        ImVec2(rowCursor.x + labelSize.x + ImGui::GetStyle().ItemSpacing.x,
               rowCursor.y));

    // if the mesh exists render the preview image for it
    if (previewAsset && previewAsset->previewTexture != VK_NULL_HANDLE) {
      ImGui::Image((ImTextureID)previewAsset->previewTexture,
                   ImVec2(previewSize, previewSize));
    } else {
      // otherwise just draw a box and some text
      ImVec2 p = ImGui::GetCursorScreenPos();
      ImDrawList *drawList = ImGui::GetWindowDrawList();
      drawList->AddRectFilled(p, ImVec2(p.x + previewSize, p.y + previewSize),
                              IM_COL32(70, 70, 70, 255), 4.0f);
      const char *text = "No mesh \n selected";
      ImVec2 textSize = ImGui::CalcTextSize(text);
      drawList->AddText(ImVec2(p.x + (previewSize - textSize.x) * 0.5f,
                               p.y + (previewSize - textSize.y) * 0.5f),
                        IM_COL32(200, 200, 200, 255), text);
      ImGui::Dummy(ImVec2(previewSize, previewSize));
    }

    // right clickable context menu for the image preview
    if (ImGui::BeginPopupContextItem("mesh_preview_context_menu")) {
      if (ImGui::Button("Remove Mesh")) {
        mesh = Handle<MeshAsset>{};
        overrideColor = glm::vec4(0.0f);
        ImGui::CloseCurrentPopup();
      }
      ImGui::Separator();

      // TODO: move this into a popup menu?
      // I dont really want this as a right click menu thing,
      // maybe a godot style model picker
      // if (modelLoader) {
      //   for (const auto &[name, _] : modelLoader->getAssets()) {
      //     if (ImGui::MenuItem(name.c_str(), "", mesh.assetName == name)) {
      //       mesh.assetName = name;
      //       resolve(*assetManager, mesh);
      //     }
      //   }
      // }
      ImGui::EndPopup();
    }

    // drag support for the asset type
    if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload *payload =
              ImGui::AcceptDragDropPayload(AssetDragPayload::kPayloadType)) {
        const auto &drag =
            *static_cast<const AssetDragPayload *>(payload->Data);
        if (drag.type == AssetDragPayload::Mesh && modelLoader &&
            modelLoader->tryGetAsset(drag.name)) {
          mesh.assetName = drag.name;
          resolve(*assetManager, mesh);
        }
      }
      ImGui::EndDragDropTarget();
    }

    ImGui::ColorEdit4("Override Color", &overrideColor.x);
  }
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(MeshComponent, mesh, overrideColor)
};
