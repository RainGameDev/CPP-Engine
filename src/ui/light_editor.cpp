
#include "ecs/components.h"
#include "ecs/entity.h"
#include "ecs/light.h"
#include "ecs/query.h"
#include "ecs/system_registry.h"
#include "ecs/world.h"

#include "imgui.h"
#include <print>
#include <string>

struct EditorStatus {
  EntityId selectedID;
};

void lightInspector(World &world,
                    Query<TransformComponent, LightComponent> query) {
  query.for_each(
      [&](EntityId id, TransformComponent &trans, LightComponent &light) {
        ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(260.0f, 0.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Light Inspector");

        ImGui::SeparatorText("Transform");
        ImGui::DragFloat3("Position", &trans.position.x, 0.1f);
        ImGui::DragFloat3("Rotation", &trans.rotation.x, 0.1f);
        ImGui::DragFloat3("Scale", &trans.scale.x, 0.1f);

        ImGui::SeparatorText("Light");
        ImGui::ColorEdit3("Color", &light.color.x);
        ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 100.0f);
        ImGui::Checkbox("Emitting", &light.isEmitting);

        const char *types[] = {"Directional", "Point", "Spot"};
        int currentType = light.lightType.index();
        if (ImGui::Combo("Type", &currentType, types, 3)) {
          if (currentType == 0)
            light.lightType = Directional{};
          else if (currentType == 1)
            light.lightType = Point{10.0f};
          else
            light.lightType = Spot{45.0f, 20.0f};
        }

        if (std::holds_alternative<Point>(light.lightType)) {
          ImGui::DragFloat("Radius", &std::get<Point>(light.lightType).radius,
                           0.1f, 0.0f, 1000.0f);
        } else if (std::holds_alternative<Spot>(light.lightType)) {
          ImGui::DragFloat("Angle", &std::get<Spot>(light.lightType).angle,
                           1.0f, 1.0f, 180.0f);
          ImGui::DragFloat("Length", &std::get<Spot>(light.lightType).length,
                           0.1f, 0.0f, 1000.0f);
        }
      });
  ImGui::End();
}
UPDATE_SYSTEM(lightInspector);

void hierarchy(World &world) {
  ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(260.0f, 0.0f), ImGuiCond_FirstUseEver);
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
  ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(260.0f, 0.0f), ImGuiCond_FirstUseEver);
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
