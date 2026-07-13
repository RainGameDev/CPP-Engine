
#include "assets/asset_manager.h"
#include "assets/material_loader.h"
#include "assets/model_loader.h"
#include "assets/shader_loader.h"
#include "ecs/components.h"
#include "ecs/entity.h"
#include "ecs/light.h"
#include "ecs/query.h"
#include "ecs/system_registry.h"
#include "ecs/world.h"
#include "rendering/vulkan_render_context.h"

#include "imgui.h"
#include "imgui_internal.h"
#include <cstdint>
#include <print>
#include <string>

struct EditorStatus {
  EntityId selectedID;
  IAssetLoader *selectedAssetLoader = nullptr;
  bool isViewportHovered = false;
};

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
      if (ImGui::MenuItem("New")) { /* ... */ }
      if (ImGui::MenuItem("Open...")) { /* ... */ }
      if (ImGui::MenuItem("Save")) { /* ... */ }
      ImGui::Separator();
      if (ImGui::MenuItem("Exit")) { /* ... */ }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
      if (ImGui::MenuItem("Undo", "Ctrl+Z")) { /* ... */ }
      if (ImGui::MenuItem("Redo", "Ctrl+Y")) { /* ... */ }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
      ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
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
                                &bottom);
    ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Down, 0.25f, &bottom,
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

  if (!world.has_resource<EditorStatus>()) {
    world.add_resource<EditorStatus>();
  }

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

  if (!world.has_resource<EditorStatus>()) {
    world.add_resource<EditorStatus>();
  }

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

  int assetCount = 0;
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
  ImGui::Text("Files (%d)", assetCount);
  ImGui::Separator();

  if (editorState.selectedAssetLoader == nullptr) {
    for (auto &entry : allAssets) {
      ImGui::Text("%s", entry.name.c_str());
    }
  } else {
    for (auto &entry : editorState.selectedAssetLoader->getAllAssetEntries()) {
      ImGui::Text("%s", entry.name.c_str());
    }
  }
  ImGui::EndChild();

  ImGui::End();
}
UPDATE_SYSTEM(assets);

void viewport(World &world) {
  auto *renderer = world.get_resource<VulkanRenderingContext *>();
  if (!renderer)
    return;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoBackground);
  ImVec2 size = ImGui::GetContentRegionAvail();
  if (size.x > 0 && size.y > 0) {
    (*renderer)->pendingViewportExtent = {static_cast<uint32_t>(size.x),
                                         static_cast<uint32_t>(size.y)};
    ImGui::Image(
        reinterpret_cast<ImTextureID>((*renderer)->viewportDescriptorSet),
        size);
  }
  ImGui::End();
  ImGui::PopStyleVar();
}
UPDATE_SYSTEM(viewport);
