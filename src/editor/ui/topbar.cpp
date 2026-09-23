#include "assets/scene_asset.h"
#include "ecs/world.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_vulkan.h"
#include "ui/editor.h"
#include "ui/shared.h"
#include <GLFW/glfw3.h>
#include <cstring>
#include <filesystem>

void topbar(World &world) {
  EditorStatus *state = world.get_resource<EditorStatus>();
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

      if (ImGui::MenuItem("Project Settings"))
        state->isProjectSettingsOpen = true;

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
      if (ImGui::MenuItem("Preferences"))
        undoScene(world);

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

      ImGuiID right, bottom, central;
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
