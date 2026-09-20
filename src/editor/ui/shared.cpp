#include "ui/shared.h"
#include "assets/asset_manager.h"
#include "ecs/components.h"
#include "ecs/entity.h"
#include "ecs/input_manager.h"
#include "ecs/serialize.h"
#include "ecs/system_registry.h"
#include "ecs/world.h"
#include "imgui.h"
#include "ui/editor.h"
#include <GLFW/glfw3.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <print>
#include <sstream>

using json = nlohmann::json;

ImGuiStyle base_style;

char sceneNameBuf[128] = "";
char nameEntityBuf[128] = "";
bool openSaveAsPopup = false;
bool openNewScenePopup = false;
PendingSceneAction pendingSceneAction = PendingSceneAction::None;

std::vector<json> undoStack;
std::vector<json> redoStack;

void saveCurrentScene(World &world) {
  json saved = save_world(world);
  std::filesystem::create_directories("assets/scenes");
  std::string path =
      "assets/scenes/" + world.currentScene.sceneName + ".scene.json";
  std::ofstream file(path);
  file << saved.dump(2);
  std::print("Saved scene to {}\n", path);
}

bool sceneIsEdited(World &world) {
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

void recordUndo(World &world) {
  undoStack.push_back(save_world(world));
  if (undoStack.size() > 64)
    undoStack.erase(undoStack.begin());
  redoStack.clear();
}

void undoScene(World &world) {
  if (undoStack.empty())
    return;
  redoStack.push_back(save_world(world));
  restore_world(world, undoStack.back());
  undoStack.pop_back();
}

void redoScene(World &world) {
  if (redoStack.empty())
    return;
  undoStack.push_back(save_world(world));
  restore_world(world, redoStack.back());
  redoStack.pop_back();
}

void editorKeybindInit(World &world) {
  InputManager *inputManager = world.get_resource<InputManager>();
  inputManager->addKeybind({GLFW_KEY_DELETE, GLFW_PRESS}, "EditorDelete");

  inputManager->addKeybind({GLFW_KEY_D, GLFW_PRESS}, "EditorDuplicate",
                           GLFW_MOD_CONTROL);
}
STARTUP_SYSTEM(editorKeybindInit);

void selectedEntityUsing(World &world) {
  EditorStatus *editorState = world.get_resource<EditorStatus>();
  InputManager *inputManager = world.get_resource<InputManager>();

  if (!editorState || !inputManager || editorState->selectedID == NONE)
    return;
  if (inputManager->isKeybindActive("EditorDelete")) {
    recordUndo(world);
    world.delete_entity(editorState->selectedID);
    editorState->selectedID = NONE;
  } else if (inputManager->isKeybindActive("EditorDuplicate")) {
    recordUndo(world);
    EntityId copy = world.duplicate_entity(editorState->selectedID);
    if (copy != NONE)
      editorState->selectedID = copy;
  }
}
UPDATE_SYSTEM(selectedEntityUsing);
