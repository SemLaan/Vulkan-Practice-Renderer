#include "game.h"

#include "game_rendering.h"
#include "player_controller.h"
#include "containers/darray.h"
#include "core/logger.h"
#include "core/input.h"
#include "renderer/obj_loader.h"
#include "marching_cubes.h"
#include "math/lin_alg.h"



GameState* gameState = nullptr;



void GameInit()
{
    gameState = Alloc(GetGlobalAllocator(), sizeof(*gameState), MEM_TAG_GAME);

    gameState->mouseEnabled = false;
    gameState->mouseEnableButtonPressed = false;
    gameState->perspectiveEnabled = true;
    gameState->destroyDebugMenu2 = false;    

    // Creating debug menu
    gameState->debugMenu = DebugUICreateMenu();
    DebugUIAddButton(gameState->debugMenu, "test", nullptr, &gameState->mouseEnableButtonPressed);
    DebugUIAddButton(gameState->debugMenu, "test2", nullptr, nullptr);
    DebugUIAddSlider(gameState->debugMenu, "mouse move speed", 1, 10000, &gameState->mouseMoveSpeed);

    // Testing multiple debug menus
    gameState->debugMenu2 = DebugUICreateMenu();
    DebugUIAddButton(gameState->debugMenu2, "test", nullptr, &gameState->mouseEnableButtonPressed);
    DebugUIAddButton(gameState->debugMenu2, "test2", nullptr, &gameState->destroyDebugMenu2);
    DebugUIAddSlider(gameState->debugMenu2, "mouse move speed", 1, 10000, &gameState->mouseMoveSpeed);

    StartOrResetTimer(&gameState->timer);

	GameRenderingInit();
	PlayerControllerInit();
}

void GameShutdown()
{

	PlayerControllerShutdown();
	GameRenderingShutdown();

    if (gameState->debugMenu2)
        DebugUIDestroyMenu(gameState->debugMenu2);
    DebugUIDestroyMenu(gameState->debugMenu);

    Free(GetGlobalAllocator(), gameState);
}

void GameUpdateAndRender()
{
    // =========================== Update ===================================
	if (GetButtonDown(BUTTON_RIGHTMOUSEBTN) && !GetButtonDownPrevious(BUTTON_RIGHTMOUSEBTN))
	{
		MCDestroyMeshAndDensityMap();
		MCGenerateDensityMap();
		MCGenerateMesh();
	}

    if (gameState->destroyDebugMenu2 && gameState->debugMenu2)
    {
        DebugUIDestroyMenu(gameState->debugMenu2);
        gameState->debugMenu2 = nullptr;
    }

    

    // If the mouse button enable button is pressed or if the mouse is enabled and the player presses it.
    if ((gameState->mouseEnableButtonPressed) ||
        (gameState->mouseEnabled && GetButtonDown(BUTTON_LEFTMOUSEBTN) && !GetButtonDownPrevious(BUTTON_LEFTMOUSEBTN)))
    {
        gameState->mouseEnabled = !gameState->mouseEnabled;
        InputSetMouseCentered(gameState->mouseEnabled);
        gameState->mouseEnableButtonPressed = false;
    }

	PlayerControllerUpdate();
	GameRenderingRender();
}
