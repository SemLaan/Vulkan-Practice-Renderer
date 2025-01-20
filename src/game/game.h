#pragma once
#include "defines.h"

#include "renderer/renderer.h"
#include "renderer/texture.h"
#include "renderer/ui/debug_ui.h"
#include "core/timer.h"




typedef struct GameState
{
    Timer timer;
    DebugMenu* debugMenu;
    DebugMenu* debugMenu2;
    f32 mouseMoveSpeed;
    bool mouseEnabled;
    bool mouseEnableButtonPressed;
    bool perspectiveEnabled;
    bool destroyDebugMenu2;
} GameState;

extern GameState* gameState;


void GameInit();

void GameUpdateAndRender();

void GameShutdown();
