#pragma once
#include "defines.h"

#include "renderer/renderer.h"
#include "renderer/texture.h"
#include "renderer/ui/debug_ui.h"
#include "core/timer.h"




typedef struct GameState
{
    Timer timer;
	Arena frameArena;
} GameState;

extern GameState* gameState;


void GameInit();

void GameUpdateAndRender();

void GameShutdown();
