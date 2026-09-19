#include "assets/asset_loader.h"
#include "assets/asset_manager.h"
#include "assets/asset_payload.h"
#include "assets/material_loader.h"
#include "assets/model_loader.h"
#include "assets/scene_asset.h"
#include "assets/shader_loader.h"
#include "assets/texture_loader.h"
#include "ecs/serialize.h"
#include "ecs/system_registry.h"
#include "ecs/world.h"
#include "imgui.h"
#include "ui/editor.h"
#include "ui/shared.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

static std::string thumbnailDragAsset;

static std::string wrapTextManual(const std::string &text, float wrapWidth) {
  std::string result, currentLine;
  size_t i = 0;
  while (i < text.size()) {
    std::string trial = currentLine + text[i];
    if (!currentLine.empty() &&
        ImGui::CalcTextSize(trial.c_str()).x > wrapWidth) {
      result += currentLine;
      result += '\n';
      currentLine.clear();
    } else {
      currentLine += text[i];
      i++;
    }
  }
  result += currentLine;
  return result;
}

template <typename ThumbnailFn>
static void drawAssetEntry(EditorStatus &editorState,
                           const IAssetLoader::AssetEntry &entry,
                           AssetDragPayload::Type dragType, float groupHeight,
                           ThumbnailFn &&drawThumbnail,
                           std::function<void()> onDoubleClick = {}) {
  ImVec2 groupSize(128, groupHeight);

  bool isSelected = (editorState.selectedAsset == entry.name);

  ImGui::PushID(entry.name.c_str());
  ImVec2 startPos = ImGui::GetCursorScreenPos();

  // base double click
  bool doubleClicked = false;
  if (ImGui::Selectable("##sel", isSelected,
                        ImGuiSelectableFlags_AllowDoubleClick, groupSize)) {
    editorState.selectedAsset = entry.name;
    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      doubleClicked = true;
    }
  }
  if (ImGui::IsItemHovered() &&
      ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    doubleClicked = true;
  }

  // Drag and drop functionality
  if (ImGui::BeginDragDropSource()) {
    AssetDragPayload payload;
    payload.type = dragType;
    strncpy(payload.name, entry.name.c_str(), sizeof(payload.name) - 1);
    ImGui::SetDragDropPayload(AssetDragPayload::kPayloadType, &payload,
                              sizeof(payload));
    if (ImGui::BeginTooltip()) {
      ImGui::Text("%s", entry.name.c_str());
      ImGui::EndTooltip();
    }
    ImGui::EndDragDropSource();
  }

  // Icon stuff
  ImGui::SetCursorScreenPos(startPos);
  ImGui::BeginGroup();
  drawThumbnail();
  ImGui::SetCursorScreenPos(ImVec2(startPos.x, startPos.y + 128.0f));
  std::string wrapped = wrapTextManual(entry.name, 128.0f);
  ImGui::TextUnformatted(wrapped.c_str());
  ImGui::EndGroup();

  // Double click fallback based
  if (doubleClicked == false && onDoubleClick &&
      ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
      ImGui::IsWindowHovered()) {
    ImVec2 mousePos = ImGui::GetMousePos();
    if (mousePos.x >= startPos.x && mousePos.y >= startPos.y &&
        mousePos.x < startPos.x + groupSize.x &&
        mousePos.y < startPos.y + groupSize.y) {
      doubleClicked = true;
    }
  }
  if (doubleClicked && onDoubleClick) {
    onDoubleClick();
  }
  ImGui::PopID();
}

static float assetNameHeight(const IAssetLoader::AssetEntry &entry) {
  std::string wrapped = wrapTextManual(entry.name, 128.0f);
  int lines = 1 + (int)std::count(wrapped.begin(), wrapped.end(), '\n');

  return lines * ImGui::GetTextLineHeight() + ImGui::GetStyle().ItemSpacing.y;
}

static void drawLabeledThumbnail(const char *label, ImU32 color) {
  ImVec2 iconMin = ImGui::GetCursorScreenPos();
  ImVec2 iconMax(iconMin.x + 128, iconMin.y + 128);
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(iconMin, iconMax, color, 4.0f);
  ImVec2 textSize = ImGui::CalcTextSize(label);
  ImVec2 textPos(iconMin.x + (128 - textSize.x) * 0.5f,
                 iconMin.y + (128 - textSize.y) * 0.5f);
  drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), label);
  ImGui::Dummy(ImVec2(128, 128));
}

static void drawPlaceholderThumbnail(const char *label) {
  drawLabeledThumbnail(label, IM_COL32(70, 70, 70, 255));
}

static void loadSceneAsset(World &world, AssetManager &assetManager,
                           EditorStatus &editorState,
                           const std::string &sceneName) {
  auto *sceneLoader =
      dynamic_cast<SceneLoader *>(assetManager.getLoader<SceneAsset>());
  if (sceneLoader == nullptr) {
    return;
  }
  auto *sceneAsset = sceneLoader->tryGetAsset(sceneName);
  if (sceneAsset == nullptr) {
    return;
  }
  std::ifstream file(sceneAsset->path);
  if (file.good() == false) {
    return;
  }
  recordUndo(world);
  restore_world(world, json::parse(file));
  editorState.selectedID = NONE;
  editorState.selectedAsset = sceneName;
}

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
  if (ImGui::Button("Scenes")) {
    editorState.selectedAssetLoader = assetManager.getLoader<SceneAsset>();
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
  std::vector<const IAssetLoader::AssetEntry *> visibleEntries;
  for (auto &entry : allAssets) {
    if (editorState.selectedAssetLoader != nullptr &&
        entry.loaderFrom != editorState.selectedAssetLoader)
      continue;

    if (editorState.assetSearch != "" &&
        !entry.name.contains(editorState.assetSearch)) {
      continue;
    }

    if (auto *texLoader =
            dynamic_cast<const TextureLoader *>(entry.loaderFrom)) {
      if (!texLoader->getAsset(entry.name).imguiDS)
        continue;
    }
    visibleEntries.push_back(&entry);
  }

  float windowVisibleX2 =
      ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
  float windowVisibleX1 =
      ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMin().x;
  float spacingX = ImGui::GetStyle().ItemSpacing.x;

  std::vector<std::vector<const IAssetLoader::AssetEntry *>> rows;
  rows.emplace_back();
  float cursorX = windowVisibleX1;
  for (auto *entry : visibleEntries) {
    float tileEndX = cursorX + 128.0f;
    if (!rows.back().empty() && tileEndX >= windowVisibleX2) {
      rows.emplace_back();
      cursorX = windowVisibleX1;
    }
    rows.back().push_back(entry);
    cursorX += 128.0f + spacingX;
  }

  for (auto &row : rows) {
    float rowTextHeight = 0.0f;
    for (auto *entry : row)
      rowTextHeight = std::max(rowTextHeight, assetNameHeight(*entry));
    float groupHeight = 128.0f + rowTextHeight;

    for (size_t i = 0; i < row.size(); i++) {
      auto *entry = row[i];
      if (auto *texLoader =
              dynamic_cast<const TextureLoader *>(entry->loaderFrom)) {
        const TextureAsset &asset = texLoader->getAsset(entry->name);
        drawAssetEntry(editorState, *entry, AssetDragPayload::Texture,
                       groupHeight, [&asset]() {
                         ImGui::Image((ImTextureID)asset.imguiDS,
                                      ImVec2(128, 128));
                       });
      } else if (entry->loaderFrom == assetManager.getLoader<MeshAsset>()) {
        auto *modelLoader =
            dynamic_cast<ModelLoader *>(assetManager.getLoader<MeshAsset>());
        MeshAsset &asset = modelLoader->getAsset(entry->name);
        drawAssetEntry(editorState, *entry, AssetDragPayload::Mesh, groupHeight,
                       [&asset, entry, modelLoader]() {
                         ImGui::Image((ImTextureID)asset.previewTexture,
                                      ImVec2(MeshAsset::kPreviewSize,
                                             MeshAsset::kPreviewSize));

                         if (ImGui::IsItemHovered() &&
                             ImGui::IsMouseDown(ImGuiMouseButton_Right))
                           thumbnailDragAsset = entry->name;
                         if (thumbnailDragAsset == entry->name) {
                           if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                             ImVec2 delta = ImGui::GetIO().MouseDelta;
                             asset.previewYaw += delta.x * 0.35f;
                             asset.previewPitch = glm::clamp(
                                 asset.previewPitch + delta.y * 0.35f, -89.0f,
                                 89.0f);
                             modelLoader->refreshPreview(entry->name);
                           } else {
                             thumbnailDragAsset.clear();
                           }
                         }
                       });
      } else if (dynamic_cast<const ShaderLoader *>(entry->loaderFrom)) {
        drawAssetEntry(
            editorState, *entry, AssetDragPayload::Shader, groupHeight,
            []() { drawLabeledThumbnail("S", IM_COL32(80, 60, 140, 255)); });
      } else if (dynamic_cast<const MaterialLoader *>(entry->loaderFrom)) {
        drawAssetEntry(
            editorState, *entry, AssetDragPayload::Material, groupHeight,
            []() { drawLabeledThumbnail("M", IM_COL32(0, 150, 140, 255)); });
      } else if (dynamic_cast<const SceneLoader *>(entry->loaderFrom)) {
        drawAssetEntry(
            editorState, *entry, AssetDragPayload::Scene, groupHeight,
            [=, &assetManager, &world, &editorState]() {
              drawLabeledThumbnail("S", IM_COL32(60, 120, 200, 255));

              if (ImGui::BeginPopupContextItem("scene_context_menu")) {
                if (ImGui::MenuItem("Load")) {
                  loadSceneAsset(world, assetManager, editorState, entry->name);
                }
                if (ImGui::MenuItem("Rename")) { /* do something */
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Delete")) { /* do something */
                }
                ImGui::EndPopup();
              }
            },
            [&world, &assetManager, &editorState, entry]() {
              loadSceneAsset(world, assetManager, editorState, entry->name);
            });
      } else {
        drawAssetEntry(editorState, *entry, AssetDragPayload::None, groupHeight,
                       []() { drawPlaceholderThumbnail("?"); });
      }

      if (i + 1 < row.size())
        ImGui::SameLine();
    }
  }

  ImGui::EndChild();
  ImGui::End();
}
UPDATE_SYSTEM(assets);
