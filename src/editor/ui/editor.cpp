#include "ui/editor.h"
#include "assets/asset_manager.h"
#include "assets/material_loader.h"
#include "assets/model_loader.h"
#include "assets/scene_asset.h"
#include "assets/shader_loader.h"
#include "assets/texture_loader.h"
#include "ecs/component_registry.h"
#include "ecs/components.h"
#include "ecs/entity.h"
#include "ecs/input_manager.h"
#include "ecs/query.h"
#include "ecs/serialize.h"
#include "ecs/system_registry.h"
#include "ecs/world.h"
#include "imgui_impl_vulkan.h"
#include "imgui_vulkan.h"
#include "rendering/vulkan_render_context.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "rendering/camera.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>
#include <print>
#include <sstream>
#include <string>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

static char sceneNameBuf[128] = "";
static char nameEntityBuf[128] = "";

ImGuiStyle base_style;
bool openSaveAsPopup = false;
bool openNewScenePopup = false;
static std::string thumbnailDragAsset;

enum class PendingSceneAction { None, New, Exit };
static PendingSceneAction pendingSceneAction = PendingSceneAction::None;

static std::vector<json> undoStack;
static std::vector<json> redoStack;

static void saveCurrentScene(World &world) {
  json saved = save_world(world);
  std::filesystem::create_directories("assets/scenes");
  std::string path =
      "assets/scenes/" + world.currentScene.sceneName + ".scene.json";
  std::ofstream file(path);
  file << saved.dump(2);
  std::print("Saved scene to {}\n", path);
}

static bool sceneIsEdited(World &world) {
  std::string path =
      "assets/scenes/" + world.currentScene.sceneName + ".scene.json";
  std::ifstream file(path);
  if (!file.good())
    return false;
  std::stringstream buffer;
  buffer << file.rdbuf();
  json disk = json::parse(buffer.str(), nullptr, false);
  if (disk.is_discarded())
    return true;
  return disk != save_world(world);
}

static void recordUndo(World &world) {
  undoStack.push_back(save_world(world));
  if (undoStack.size() > 64)
    undoStack.erase(undoStack.begin());
  redoStack.clear();
}

static void undoScene(World &world) {
  if (undoStack.empty())
    return;
  redoStack.push_back(save_world(world));
  restore_world(world, undoStack.back());
  undoStack.pop_back();
}

static void redoScene(World &world) {
  if (redoStack.empty())
    return;
  undoStack.push_back(save_world(world));
  restore_world(world, redoStack.back());
  redoStack.pop_back();
}

void topbar(World &world) {
  ImGuiViewport *mainViewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(mainViewport->WorkPos);
  ImGui::SetNextWindowSize(mainViewport->WorkSize);
  ImGui::SetNextWindowViewport(mainViewport->ID);

  ImGuiWindowFlags hostFlags =
      ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::Begin("DockSpace Host", nullptr, hostFlags);
  ImGui::PopStyleVar(3);

  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("New")) {
        if (sceneIsEdited(world)) {
          pendingSceneAction = PendingSceneAction::New;
          ImGui::OpenPopup("Unsaved Changes");
        } else {
          strncpy(sceneNameBuf, "Scene", sizeof(sceneNameBuf));
          sceneNameBuf[sizeof(sceneNameBuf) - 1] = '\0';
          openNewScenePopup = true;
        }
      }
      if (ImGui::MenuItem("Save")) {
        saveCurrentScene(world);
      }
      if (ImGui::MenuItem("Save As")) {
        strncpy(sceneNameBuf, world.currentScene.sceneName.c_str(),
                sizeof(sceneNameBuf));
        sceneNameBuf[sizeof(sceneNameBuf) - 1] = '\0';
        openSaveAsPopup = true;
      }

      ImGui::Separator();
      if (ImGui::MenuItem("Exit")) {
        if (sceneIsEdited(world)) {
          pendingSceneAction = PendingSceneAction::Exit;
          ImGui::OpenPopup("Unsaved Changes");
        } else {
          auto *window = world.get_resource<GLFWwindow *>();
          glfwSetWindowShouldClose(*window, GLFW_TRUE);
        }
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
      if (ImGui::MenuItem("Undo", "Ctrl+Z"))
        undoScene(world);
      if (ImGui::MenuItem("Redo", "Ctrl+Y"))
        redoScene(world);
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
      ImGui::SliderFloat("UI Scale", &ui_scale, 0.25f, 3.0f);
      ImGuiStyle &style = ImGui::GetStyle();
      style = base_style;
      style.ScaleAllSizes(ui_scale);
      ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
  }

  ImGuiIO &io = ImGui::GetIO();
  if (!io.WantTextInput) {
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z))
      undoScene(world);
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y))
      redoScene(world);
  }

  if (openNewScenePopup) {
    ImGui::OpenPopup("New Scene");
    openNewScenePopup = false;
  }
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                          ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  if (ImGui::BeginPopupModal("New Scene", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Scene name:");
    ImGui::SetNextItemWidth(250);

    bool enterPressed =
        ImGui::InputText("##newscenename", sceneNameBuf, sizeof(sceneNameBuf),
                         ImGuiInputTextFlags_EnterReturnsTrue);

    ImGui::Spacing();

    bool createPressed = ImGui::Button("Create", ImVec2(120, 0));
    ImGui::SameLine();
    bool cancelPressed = ImGui::Button("Cancel", ImVec2(120, 0));

    if ((createPressed || enterPressed) && sceneNameBuf[0] != '\0') {
      recordUndo(world);
      world.currentScene = make_blank_scene(sceneNameBuf);
      if (auto *editorState = world.get_resource<EditorStatus>())
        editorState->selectedID = NONE;
      ImGui::CloseCurrentPopup();
    }

    if (cancelPressed) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
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

      saveCurrentScene(world);

      ImGui::CloseCurrentPopup();
    }

    if (cancelPressed) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  if (pendingSceneAction != PendingSceneAction::None) {
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Unsaved Changes", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("Scene \"%s\" has unsaved changes.",
                  world.currentScene.sceneName.c_str());
      ImGui::Spacing();

      bool savePressed = ImGui::Button("Save", ImVec2(120, 0));
      ImGui::SameLine();
      bool discardPressed = ImGui::Button("Discard", ImVec2(120, 0));
      ImGui::SameLine();
      bool cancelPressed = ImGui::Button("Cancel", ImVec2(120, 0));

      if (savePressed)
        saveCurrentScene(world);
      if (discardPressed || savePressed) {
        PendingSceneAction action = pendingSceneAction;
        pendingSceneAction = PendingSceneAction::None;
        ImGui::CloseCurrentPopup();

        if (action == PendingSceneAction::New) {
          strncpy(sceneNameBuf, "Scene", sizeof(sceneNameBuf));
          sceneNameBuf[sizeof(sceneNameBuf) - 1] = '\0';
          openNewScenePopup = true;
        } else if (action == PendingSceneAction::Exit) {
          auto *window = world.get_resource<GLFWwindow *>();
          glfwSetWindowShouldClose(*window, GLFW_TRUE);
        }
      }

      if (cancelPressed) {
        pendingSceneAction = PendingSceneAction::None;
        ImGui::CloseCurrentPopup();
      }

      ImGui::EndPopup();
    }
  }

  ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
  ImGui::DockSpace(dockspaceId, ImVec2(0, 0));

  if (!std::filesystem::exists("imgui.ini")) {

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
    recordUndo(world);
    EntityId id = world.create_entity();
    world.add_component(id, NameComponent{.name = "Entity"});
  }

  ImGui::Separator();

  Query<NameComponent> nameQuery(world);
  std::vector<EntityId> entities;
  nameQuery.for_each(
      [&entities](EntityId id, NameComponent &) { entities.push_back(id); });
  std::sort(entities.begin(), entities.end());

  for (EntityId id : entities) {
    auto *nameComp = world.get_component<NameComponent>(id);
    if (!nameComp)
      continue;

    if (ImGui::Button(
            (nameComp->name + " (" + std::to_string(id) + ")").c_str())) {
      editorState.selectedID = id;
    }

    if (ImGui::BeginPopupContextItem()) {
      if (ImGui::MenuItem("Delete")) {
        world.delete_entity(id);
        ImGui::CloseCurrentPopup();
      }

      ImGui::EndPopup();
    }
  }

  ImGui::End();
}
UPDATE_SYSTEM(hierarchy);

void inspector(World &world) {
  ImGui::Begin("Inspector");

  EditorStatus &editorState = *world.get_resource<EditorStatus>();

  if (editorState.selectedID != NONE) {
    if (ImGui::Button("Add Component")) {
      ImGui::OpenPopup("ComponentAddPopup");
    }

    if (ImGui::BeginPopup("ComponentAddPopup")) {
      ImGui::Text("Select Component");
      ImGui::Separator();
      std::unordered_map<std::string, ComponentRegistry::Entry> map =
          ComponentRegistry::instance().entries();

      for (auto &[name, entry] : map) {

        if (entry.contains(world, editorState.selectedID)) {
          continue;
        }
        if (ImGui::Selectable(name.c_str())) {
          entry.add_component(world, editorState.selectedID);
          ImGui::CloseCurrentPopup();
        }
      }

      ImGui::EndPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Delete")) {
      world.delete_entity(editorState.selectedID);
    }
    ImGui::Separator();

    NameComponent *nameComp =
        world.get_component<NameComponent>(editorState.selectedID);
    if (!nameComp) {
      editorState.selectedID = NONE;
    } else {

      auto name = nameComp->name.c_str();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("%s", name);
      ImGui::SameLine();
      bool enterPressed =
          ImGui::InputText("##EntityName", nameEntityBuf, sizeof(nameEntityBuf),
                           ImGuiInputTextFlags_EnterReturnsTrue);

      if (enterPressed) {
        nameComp->setName(nameEntityBuf);
        strncpy(nameEntityBuf, "", sizeof(nameEntityBuf));
      }

      world.inspect_entity(editorState.selectedID);

      static json cleanSnapshot;
      static bool capturedClean = false;
      static bool wasEditing = false;
      const bool editing = ImGui::IsAnyItemActive() &&
                           ImGui::GetCurrentContext()->ActiveIdWindow ==
                               ImGui::GetCurrentWindow();
      if (editing && !wasEditing && capturedClean) {
        redoStack.clear();
        undoStack.push_back(std::move(cleanSnapshot));
        if (undoStack.size() > 64)
          undoStack.erase(undoStack.begin());
      }
      wasEditing = editing;
      if (!editing) {
        cleanSnapshot = save_world(world);
        capturedClean = true;
      }
    }
  }

  ImGui::End();
}
UPDATE_SYSTEM(inspector);

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

/// Renders a single asset tile
template <typename ThumbnailFn>
static void drawAssetEntry(EditorStatus &editorState,
                           const IAssetLoader::AssetEntry &entry,
                           AssetDragPayload::Type dragType, float groupHeight,
                           ThumbnailFn &&drawThumbnail) {
  ImVec2 groupSize(128, groupHeight);

  bool isSelected = (editorState.selectedAsset == entry.name);

  ImGui::PushID(entry.name.c_str());
  ImVec2 startPos = ImGui::GetCursorScreenPos();

  if (ImGui::Selectable("##sel", isSelected, ImGuiSelectableFlags_None,
                        groupSize)) {
    editorState.selectedAsset = entry.name;
  }

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

  ImGui::SetCursorScreenPos(startPos);
  ImGui::BeginGroup();
  drawThumbnail();
  ImGui::SetCursorScreenPos(ImVec2(startPos.x, startPos.y + 128.0f));
  std::string wrapped = wrapTextManual(entry.name, 128.0f);
  ImGui::TextUnformatted(wrapped.c_str());
  ImGui::EndGroup();
  ImGui::PopID();
}

static float assetNameHeight(const IAssetLoader::AssetEntry &entry) {
  std::string wrapped = wrapTextManual(entry.name, 128.0f);
  int lines = 1 + (int)std::count(wrapped.begin(), wrapped.end(), '\n');

  return lines * ImGui::GetTextLineHeight() + ImGui::GetStyle().ItemSpacing.y;
}

/// Draws a colored square thumbnail with a centered letter label.
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

/// Generic placeholder thumbnail for asset types without a dedicated preview.
static void drawPlaceholderThumbnail(const char *label) {
  drawLabeledThumbnail(label, IM_COL32(70, 70, 70, 255));
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

  // Group entries into rows based on their fixed 128px tile width.
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

                         // Right-click + drag on the thumbnail rotates the
                         // model preview.
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
            [=, &assetManager, &world]() {
              drawLabeledThumbnail("S", IM_COL32(60, 120, 200, 255));

              if (ImGui::BeginPopupContextItem("scene_context_menu")) {
                if (ImGui::MenuItem("Load")) {
                  auto *sceneLoader = dynamic_cast<SceneLoader *>(
                      assetManager.getLoader<SceneAsset>());
                  std::ifstream file(sceneLoader->getAsset(entry->name).path);
                  restore_world(world, json::parse(file));
                }
                if (ImGui::MenuItem("Rename")) { /* do something */
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Delete")) { /* do something */
                }
                ImGui::EndPopup();
              }
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

/// where a dropping mesh should land in the scene
static glm::vec3 viewportDropPosition(World &world, ImVec2 rectPos,
                                      ImVec2 rectSize) {
  auto *editorCam = world.get_resource<EditorCamera>();
  if (!editorCam || rectSize.x <= 0.0f || rectSize.y <= 0.0f)
    return glm::vec3(0.0f);

  float aspect = rectSize.x / rectSize.y;
  glm::mat4 view = editorCam->cam.getViewMatrix(editorCam->transform);
  glm::mat4 proj = editorCam->cam.getProjectionMatrix(aspect);
  glm::mat4 invViewProj = glm::inverse(proj * view);

  ImVec2 mouse = ImGui::GetIO().MousePos;
  float ndcX = (mouse.x - rectPos.x) / rectSize.x * 2.0f - 1.0f;
  float ndcY = 1.0f - (mouse.y - rectPos.y) / rectSize.y * 2.0f;

  glm::vec4 farClip = invViewProj * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
  glm::vec3 far = glm::vec3(farClip) / farClip.w;
  glm::vec3 origin = editorCam->cam.getPosition(editorCam->transform);
  glm::vec3 dir = glm::normalize(far - origin);

  if (glm::abs(dir.y) > 1e-4f) {
    float t = (0.0f - origin.y) / dir.y;
    if (t > 0.0f)
      return origin + dir * t;
  }
  return origin + dir * 5.0f;
}

/// Spawns a new entity carrying a dropped mesh asset
static void spawnEntityFromMeshDrop(World &world,
                                    const AssetDragPayload &payload,
                                    glm::vec3 position) {
  auto *assetManager = world.get_resource<AssetManager>();
  auto *modelLoader =
      assetManager ? assetManager->getLoader<MeshAsset>() : nullptr;
  if (!assetManager || !modelLoader || !modelLoader->tryGetAsset(payload.name))
    return;

  recordUndo(world);
  EntityId id = world.create_entity();
  world.add_component(id, NameComponent{.name = payload.name});
  MeshComponent mc;
  mc.mesh.assetName = payload.name;
  resolve(*assetManager, mc.mesh);
  resolve(*assetManager, mc.overrideMaterial);
  world.add_component(id, std::move(mc));
  world.add_component(id, TransformComponent{.position = position});

  if (auto *status = world.get_resource<EditorStatus>())
    status->selectedID = id;
}

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
    ImVec2 imageMin = ImGui::GetCursorScreenPos();
    ImGui::Image(
        reinterpret_cast<ImTextureID>((*renderer)->viewportDescriptorSet),
        size);

    if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload *payload =
              ImGui::AcceptDragDropPayload(AssetDragPayload::kPayloadType)) {
        const auto &drag =
            *static_cast<const AssetDragPayload *>(payload->Data);
        if (drag.type == AssetDragPayload::Mesh) {
          glm::vec3 dropPos = viewportDropPosition(world, imageMin, size);
          spawnEntityFromMeshDrop(world, drag, dropPos);
        }
      }
      ImGui::EndDragDropTarget();
    }
  }

  status->isViewportHovered = ImGui::IsWindowHovered();

  ImGui::End();
  ImGui::PopStyleVar();
}
UPDATE_SYSTEM(viewport);
