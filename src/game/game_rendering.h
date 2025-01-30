#pragma once

#include "defines.h"
#include "game_rendering.h"
#include "renderer/camera.h"
#include "renderer/ui/debug_ui.h"

typedef struct GameCameras
{
	Camera* sceneCamera;
	Camera* uiCamera;
} GameCameras;

void GameRenderingInit();

void GameRenderingRender();

void GameRenderingShutdown();

GameCameras GetGameCameras();

// Registers a debug menu with the renderer so it will be rendered if the menu is active
void RegisterDebugMenu(DebugMenu* debugMenu);
// Unregisters a debug menu with the renderer. Should be called when the debug menu is destroyed (just before or after doesn't matter).
void UnregisterDebugMenu(DebugMenu* debugMenu);
