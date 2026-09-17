#pragma once

#include "ecs/world.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

extern char sceneNameBuf[128];
extern char nameEntityBuf[128];
extern bool openSaveAsPopup;
extern bool openNewScenePopup;

enum class PendingSceneAction { None, New, Exit };
extern PendingSceneAction pendingSceneAction;

extern std::vector<nlohmann::json> undoStack;
extern std::vector<nlohmann::json> redoStack;

void saveCurrentScene(World &world);
bool sceneIsEdited(World &world);
void recordUndo(World &world);
void undoScene(World &world);
void redoScene(World &world);
