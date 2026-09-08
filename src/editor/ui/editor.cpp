#include "ui/editor.h"
#include "assets/asset_manager.h"
#include "assets/material_loader.h"
#include "assets/model_loader.h"
#include "assets/shader_loader.h"
#include "assets/texture_loader.h"
#include "ecs/components.h"
#include "ecs/entity.h"
#include "ecs/input_manager.h"
#include "ecs/query.h"
#include "ecs/serialize.h"
#include "ecs/system_registry.h"
#include "ecs/world.h"
#include "imgui_impl_vulkan.h"
#include "rendering/vulkan_render_context.h"

#include "imgui.h"
#include "imgui_internal.h"
#include <GLFW/glfw3.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>
#include <print>
#include <string>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

static char sceneNameBuf[128] = "";
bool openSaveAsPopup = false;
static std::string thumbnailDragAsset;

void topbar(World &world) {
  ImGuiViewport *mainViewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(mainViewport->WorkPos);
  ImGui::SetNextWindowSize(mainViewport->WorkSize);
  ImGui::SetNextWindowViewport(mainViewport->ID);

  ImGuiWindowFlags hostFlags =
      ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoNavFocus;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::Begin("DockSpace Host", nullptr, hostFlags);
  ImGui::PopStyleVar(3);

  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      // TODO: do a check on if the current scene has been edited by comparing
      // it to it's save file (if it exists)
      if (ImGui::MenuItem("New")) {
        world.currentScene = make_blank_scene();
      }
      if (ImGui::MenuItem("Save")) {
        json saved = save_world(world);
        std::filesystem::create_directories("assets");
        std::string path =
            "assets/" + world.currentScene.sceneName + ".scene.json";
        std::ofstream file(path);
        file << saved.dump(2);
        std::print("Saved scene to {}\n", path);
      }
      if (ImGui::MenuItem("Save As")) {
        strncpy(sceneNameBuf, world.currentScene.sceneName.c_str(),
                sizeof(sceneNameBuf));
        sceneNameBuf[sizeof(sceneNameBuf) - 1] = '\0';
        openSaveAsPopup = true;
      }

      ImGui::Separator();
      // TODO: do a check on if the current scene has been edited by comparing
      // it to it's save file (if it exists)
      if (ImGui::MenuItem("Exit")) {
        auto *window = world.get_resource<GLFWwindow *>();
        glfwSetWindowShouldClose(*window, GLFW_TRUE);
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
      if (ImGui::MenuItem("Undo", "Ctrl+Z")) { /* ... */
      }
      if (ImGui::MenuItem("Redo", "Ctrl+Y")) { /* ... */
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
      ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
  }

  if (openSaveAsPopup) {
    ImGui::OpenPopup("Save As");
    openSaveAsPopup = false;
  }
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                          ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  if (ImGui::BeginPopupModal("Save As", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Scene name:");
    ImGui::SetNextItemWidth(250);

    bool enterPressed =
        ImGui::InputText("##scenename", sceneNameBuf, sizeof(sceneNameBuf),
                         ImGuiInputTextFlags_EnterReturnsTrue);

    ImGui::Spacing();

    bool savePressed = ImGui::Button("Save", ImVec2(120, 0));
    ImGui::SameLine();
    bool cancelPressed = ImGui::Button("Cancel", ImVec2(120, 0));

    if ((savePressed || enterPressed) && sceneNameBuf[0] != '\0') {
      world.currentScene.sceneName = sceneNameBuf;

      json saved = save_world(world);
      std::filesystem::create_directories("assets");
      std::string path =
          "assets/" + world.currentScene.sceneName + ".scene.json";
      std::ofstream file(path);
      file << saved.dump(2);
      std::print("Saved scene to {}\n", path);

      ImGui::CloseCurrentPopup();
    }

    if (cancelPressed) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
  ImGui::DockSpace(dockspaceId, ImVec2(0, 0));

  static bool dockspaceInitialized = false;
  if (!dockspaceInitialized) {
    dockspaceInitialized = true;

    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, mainViewport->Size);

    ImGuiID right;
    ImGuiID bottom;
    ImGuiID central;
    ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Right, 0.25f, &right,
                                &central);
    ImGui::DockBuilderSplitNode(central, ImGuiDir_Down, 0.25f, &bottom,
                                &central);

    ImGui::DockBuilderDockWindow("Assets", bottom);
    ImGui::DockBuilderDockWindow("Hierarchy", right);
    ImGui::DockBuilderDockWindow("Inspector", right);
    ImGui::DockBuilderDockWindow("Viewport", central);

    ImGui::DockBuilderFinish(dockspaceId);
  }

  ImGui::End();
}
UPDATE_SYSTEM(topbar);

void hierarchy(World &world) {
  ImGui::Begin("Hierarchy");

  std::string scene_name = "Current scene: " + world.currentScene.sceneName;
  ImGui::Text("%s", scene_name.c_str());
  ImGui::Separator();

  EditorStatus &editorState = *world.get_resource<EditorStatus>();

  if (ImGui::Button("New Entity")) {
    world.create_entity();
  }

  ImGui::Separator();

  Query<NameComponent> nameQuery(world);
  nameQuery.for_each([&editorState](EntityId id, NameComponent &nameComp) {
    if (ImGui::Button(
            (nameComp.name + " (" + std::to_string(id) + ")").c_str())) {
      editorState.selectedID = id;
    }
  });

  ImGui::End();
}
UPDATE_SYSTEM(hierarchy);

void inspector(World &world) {
  ImGui::Begin("Inspector");

  EditorStatus &editorState = *world.get_resource<EditorStatus>();

  if (editorState.selectedID) {
    std::string name =
        world.get_component<NameComponent>(editorState.selectedID)->name;
    ImGui::Text("%s", name.c_str());

    world.inspect_entity(editorState.selectedID);

    ImGui::Separator();
  }

  ImGui::End();
}
UPDATE_SYSTEM(inspector);

void assets(World &world) {
  EditorStatus &editorState = *world.get_resource<EditorStatus>();
  AssetManager &assetManager = *world.get_resource<AssetManager>();
  std::vector<IAssetLoader::AssetEntry> allAssets;
  for (auto &loader : assetManager.loaders) {
    auto entries = loader->getAllAssetEntries();
    allAssets.insert(allAssets.end(), entries.begin(), entries.end());
  }
  ImGui::Begin("Assets");
  ImGui::BeginChild("left pane", ImVec2(150, 0), true);
  ImGui::Text("Categories");
  ImGui::Separator();
  if (ImGui::Button("All")) {
    editorState.selectedAssetLoader = nullptr;
  }
  if (ImGui::Button("Materials")) {
    editorState.selectedAssetLoader = assetManager.getLoader<MaterialAsset>();
  }
  if (ImGui::Button("Models")) {
    editorState.selectedAssetLoader = assetManager.getLoader<MeshAsset>();
  }
  if (ImGui::Button("Shaders")) {
    editorState.selectedAssetLoader = assetManager.getLoader<ShaderAsset>();
  }
  if (ImGui::Button("Textures")) {
    editorState.selectedAssetLoader = assetManager.getLoader<TextureAsset>();
  }
  ImGui::EndChild();
  ImGui::SameLine();
  ImGui::BeginChild("right pane", ImVec2(0, 0), true);
  static char nameBuf[256];
  strncpy(nameBuf, editorState.assetSearch.c_str(), sizeof(nameBuf));
  ImGui::Text("Filter:");
  ImGui::SameLine();
  if (ImGui::InputText("##filter", nameBuf, sizeof(nameBuf))) {
    editorState.assetSearch = nameBuf;
  }
  ImGui::Separator();
  for (auto &entry : allAssets) {
    if (editorState.selectedAssetLoader != nullptr &&
        entry.loaderFrom != editorState.selectedAssetLoader)
      continue;

    float windowVisibleX2 =
        ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    bool isSelected = (editorState.selectedAsset == entry.name);
    ImVec2 startPos = ImGui::GetCursorScreenPos();
    float textWidth = ImGui::CalcTextSize(entry.name.c_str()).x;
    int textLines = std::max(1, (int)std::ceil(textWidth / 128.0f));
    float textHeight = textLines * ImGui::GetTextLineHeightWithSpacing();
    ImVec2 groupSize(128, 128 + textHeight);

    if (editorState.assetSearch != "" &&
        !entry.name.contains(editorState.assetSearch)) {
      continue;
    }

    if (auto *texLoader =
            dynamic_cast<const TextureLoader *>(entry.loaderFrom)) {
      const TextureAsset &asset = texLoader->getAsset(entry.name);
      if (!asset.imguiDS)
        continue;

      ImGui::PushID(entry.name.c_str());

      if (ImGui::Selectable("##sel", isSelected, ImGuiSelectableFlags_None,
                            groupSize)) {
        editorState.selectedAsset = entry.name;
      }

      ImGui::SetCursorScreenPos(startPos);
      ImGui::BeginGroup();
      ImGui::Image((ImTextureID)asset.imguiDS, ImVec2(128, 128));
      ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 128);
      ImGui::TextWrapped("%s", entry.name.c_str());
      ImGui::PopTextWrapPos();
      ImGui::EndGroup();

      ImGui::PopID();
    }

    else if (auto *modelLoader =
                 dynamic_cast<ModelLoader *>(assetManager.getLoader<MeshAsset>());
             entry.loaderFrom == assetManager.getLoader<MeshAsset>()) {
      MeshAsset &asset = modelLoader->getAsset(entry.name);
      ImGui::PushID(entry.name.c_str());

      if (ImGui::Selectable("##sel", isSelected, ImGuiSelectableFlags_None,
                            groupSize)) {
        editorState.selectedAsset = entry.name;
      }

      ImGui::SetCursorScreenPos(startPos);
      ImGui::BeginGroup();

      ImGui::Image((ImTextureID)asset.previewTexture,
                   ImVec2(MeshAsset::kPreviewSize, MeshAsset::kPreviewSize));

      // Right-click + drag on the thumbnail rotates the model preview.
      if (ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Right))
        thumbnailDragAsset = entry.name;
      if (thumbnailDragAsset == entry.name) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
          ImVec2 delta = ImGui::GetIO().MouseDelta;
          asset.previewYaw += delta.x * 0.35f;
          asset.previewPitch = glm::clamp(asset.previewPitch + delta.y * 0.35f,
                                          -89.0f, 89.0f);
          modelLoader->refreshPreview(entry.name);
        } else {
          thumbnailDragAsset.clear();
        }
      }

      ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 128);
      ImGui::TextWrapped("%s", entry.name.c_str());
      ImGui::PopTextWrapPos();
      ImGui::EndGroup();

      ImGui::PopID();
    }

    else if (dynamic_cast<const ShaderLoader *>(entry.loaderFrom)) {
      ImGui::PushID(entry.name.c_str());

      if (ImGui::Selectable("##sel", isSelected, ImGuiSelectableFlags_None,
                            groupSize)) {
        editorState.selectedAsset = entry.name;
      }

      ImGui::SetCursorScreenPos(startPos);
      ImGui::BeginGroup();
      ImVec2 iconMin = ImGui::GetCursorScreenPos();
      ImVec2 iconMax(iconMin.x + 128, iconMin.y + 128);
      ImDrawList *drawList = ImGui::GetWindowDrawList();
      drawList->AddRectFilled(iconMin, iconMax, IM_COL32(80, 60, 140, 255),
                              4.0f);
      ImVec2 textSize = ImGui::CalcTextSize("S");
      ImVec2 textPos(iconMin.x + (128 - textSize.x) * 0.5f,
                     iconMin.y + (128 - textSize.y) * 0.5f);
      drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), "S");
      ImGui::Dummy(ImVec2(128, 128));
      ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 128);
      ImGui::TextWrapped("%s", entry.name.c_str());
      ImGui::PopTextWrapPos();
      ImGui::EndGroup();

      ImGui::PopID();
    }
    float lastItemX2 = ImGui::GetItemRectMax().x;
    float nextItemX2 = lastItemX2 + ImGui::GetStyle().ItemSpacing.x + 128.0f;
    if (nextItemX2 < windowVisibleX2)
      ImGui::SameLine();
  }

  ImGui::EndChild();
  ImGui::End();
}
UPDATE_SYSTEM(assets);

void viewport(World &world) {
  auto *renderer = world.get_resource<VulkanRenderingContext *>();
  auto *status = world.get_resource<EditorStatus>();
  if (!renderer || !status)
    return;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::Begin("Viewport", nullptr,
               ImGuiWindowFlags_NoBackground |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);
  ImVec2 size = ImGui::GetContentRegionAvail();
  if (size.x > 0 && size.y > 0) {
    (*renderer)->pendingViewportExtent = {static_cast<uint32_t>(size.x),
                                          static_cast<uint32_t>(size.y)};
    ImGui::Image(
        reinterpret_cast<ImTextureID>((*renderer)->viewportDescriptorSet),
        size);
  }

  status->isViewportHovered = ImGui::IsWindowHovered();

  ImGui::End();
  ImGui::PopStyleVar();
}
UPDATE_SYSTEM(viewport);

