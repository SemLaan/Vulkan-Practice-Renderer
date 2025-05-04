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


