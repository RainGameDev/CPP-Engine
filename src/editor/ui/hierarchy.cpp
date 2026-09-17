#include "ecs/components.h"
#include "ecs/query.h"
#include "ecs/system_registry.h"
#include "ecs/world.h"
#include "imgui.h"
#include "ui/editor.h"
#include "ui/shared.h"
#include <algorithm>
#include <string>
#include <vector>

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
