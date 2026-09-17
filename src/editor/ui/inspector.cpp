#include "ecs/component_registry.h"
#include "ecs/components.h"
#include "ecs/serialize.h"
#include "ecs/world.h"
#include "imgui.h"
#include "ui/editor.h"
#include "imgui_internal.h"
#include "ui/shared.h"
#include <cstring>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

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
