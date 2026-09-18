#include "assets/asset_manager.h"
#include "assets/material_loader.h"
#include "ecs/system_registry.h"
#include "ecs/world.h"
#include "imgui.h"
#include "ui/editor.h"

void asset_editor(World &world) {
  ImGui::Begin("Asset Editor");
  AssetManager &assetManager = *world.get_resource<AssetManager>();
  EditorStatus &editorState = *world.get_resource<EditorStatus>();

  if (editorState.selectedAsset != "") {
    ImGui::Text("Selected an asset");
    if (editorState.selectedAsset.contains(".mat")) {
      auto &materialLoader = *assetManager.getLoader<MaterialAsset>();
      auto &asset = materialLoader.getAsset(editorState.selectedAsset);
    }
  } else {
    ImGui::Text("Select an asset to edit.");
  }

  ImGui::End();
}
UPDATE_SYSTEM(asset_editor)
