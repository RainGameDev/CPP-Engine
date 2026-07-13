
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

#include "imgui.h"
#include "imgui_internal.h"
#include <cstdint>
#include <print>
#include <string>

struct EditorStatus {
  EntityId selectedID;
  IAssetLoader *selectedAssetLoader = nullptr;
};

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
