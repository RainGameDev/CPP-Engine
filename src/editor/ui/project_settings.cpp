
#include "assets/asset_manager.h"
#include "assets/handle.h"
#include "ecs/system_registry.h"
#include "ecs/world.h"
#include "imgui.h"
#include "ui/editor.h"
#include <algorithm>
#include <cstddef>

void projectSettings(World &world) {

  AssetManager &assetManager = *world.get_resource<AssetManager>();
  EditorStatus &editorState = *world.get_resource<EditorStatus>();
  auto *texLoader = assetManager.getLoader<TextureAsset>();

  if (!editorState.isProjectSettingsOpen)
    return;

  ImGui::Begin("Project Settings");

  ImGui::BeginChild("left pane", ImVec2(150, 0), true);
  ImGui::Text("Categories");
  ImGui::Separator();

  if (ImGui::Button("Project")) {
    editorState.selectedProjectSettingCategory = "Project";
  }

  if (ImGui::Button("Graphics")) {
    editorState.selectedProjectSettingCategory = "Graphics";
  }

  if (ImGui::Button("Input")) {
    editorState.selectedProjectSettingCategory = "Input";
  }

  ImGui::EndChild();
  ImGui::SameLine();
  ImGui::BeginChild("right pane", ImVec2(0, 0), true);

  // Column aligning the inputs every widget starts at the same x, past the
  // widest label.
  const char *kSettingLabels[] = {"Filter:", "Project Name:", "Developer:",
                                  "Project Icon: ", "Default Scene: "};
  float maxLabelWidth = 0.0f;
  for (const char *label : kSettingLabels)
    maxLabelWidth = std::max(maxLabelWidth, ImGui::CalcTextSize(label).x);
  const float widgetOffsetX = maxLabelWidth + ImGui::GetStyle().ItemSpacing.x;

  static char searchBuf[256];
  static char projectNameBuf[256];
  static char developerNameBuf[256];
  strncpy(searchBuf, editorState.projectSettingSearch.c_str(),
          sizeof(searchBuf));

  // FILTER

  ImGui::Text("Filter:");
  ImGui::SameLine(widgetOffsetX);
  if (ImGui::InputText("##filter", searchBuf, sizeof(searchBuf))) {
    editorState.projectSettingSearch = searchBuf;
  }
  ImGui::Separator();

  // PROJECT

  if (editorState.selectedProjectSettingCategory == "Project") {
    ImGui::Text("Project Name:");
    ImGui::SameLine(widgetOffsetX);
    ImGui::InputText("##ProjectName", projectNameBuf, sizeof(projectNameBuf));

    ImGui::Text("Developer:");
    ImGui::SameLine(widgetOffsetX);
    ImGui::InputText("##Developer", developerNameBuf, sizeof(developerNameBuf));

    assetDragDropField(
        "Project Icon: ", AssetDragPayload::Texture, assetManager,
        editorState.projectIcon, 96.0f,

        [&](float s) {
          TextureAsset *t =
              editorState.projectIcon
                  ? texLoader->tryGetAsset(editorState.projectIcon.assetName)
                  : nullptr;
          if (t && t->imguiDS)
            ImGui::Image((ImTextureID)t->imguiDS, ImVec2(s, s));
          else {
            ImVec2 min = ImGui::GetCursorScreenPos();
            ImVec2 max(min.x + s, min.y + s);
            ImGui::GetWindowDrawList()->AddRect(min, max,
                                                IM_COL32(120, 120, 120, 255),
                                                4.0f, ImDrawFlags_None, 2.0f);
            ImGui::Dummy(ImVec2(s, s));
          }
        },
        [&]() {}, widgetOffsetX);

    assetDragDropField(
        "Default Scene: ", AssetDragPayload::Scene, assetManager,
        editorState.defaultScene, 96.0f,

        [&](float s) {
          if (editorState.defaultScene) {
            ImGui::Text("%s", editorState.defaultScene.assetName.c_str());
          } else {
            ImVec2 min = ImGui::GetCursorScreenPos();
            ImVec2 max(min.x + s, min.y + s);
            ImGui::GetWindowDrawList()->AddRect(min, max,
                                                IM_COL32(120, 120, 120, 255),
                                                4.0f, ImDrawFlags_None, 2.0f);
            ImGui::Dummy(ImVec2(s, s));
          }
        },
        [&]() {}, widgetOffsetX);
  }

  // GRAPHICS

  if (editorState.selectedProjectSettingCategory == "Graphics") {
  }

  // INPUT

  if (editorState.selectedProjectSettingCategory == "Input") {
  }

  ImGui::EndChild();
  ImGui::End();
}

UPDATE_SYSTEM(projectSettings);
