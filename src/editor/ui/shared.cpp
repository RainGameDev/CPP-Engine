#include "ui/shared.h"
#include "assets/asset_manager.h"
#include "ecs/components.h"
#include "ecs/serialize.h"
#include "ecs/world.h"
#include "imgui.h"
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
