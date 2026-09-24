#pragma once
#include "assets/handle.h"
#include "assets/scene_asset.h"
#include "assets/texture_loader.h"
#include "imgui.h"

#include "ImGuizmo.h"
#include "assets/asset_loader.h"
#include "assets/asset_payload.h"
#include "ecs/entity.h"
#include <optional>
#include <string>

class World;
void topbar(World &world);
void drawViewportGizmos(World &world, ImVec2 gizmoPos, ImVec2 gizmoSize);

struct EditorStatus {
  EntityId selectedID = NONE;
  IAssetLoader *selectedAssetLoader = nullptr;
  bool isViewportHovered = false;
  std::string assetSearch;
  std::string selectedAsset;
  std::string hoveredAsset;

  bool handTool = false;
  ImGuizmo::MODE gizmoMode = ImGuizmo::WORLD;
  bool showSceneGizmos = true;
  ImVec2 toolbarRectMin{0, 0}, toolbarRectMax{0, 0};

  ImGuizmo::OPERATION currentOp = ImGuizmo::TRANSLATE;

  bool isProjectSettingsOpen = false;
  std::string selectedProjectSettingCategory;
  std::string projectSettingSearch;

  Handle<TextureAsset> projectIcon;
  Handle<SceneAsset> defaultScene;
};
