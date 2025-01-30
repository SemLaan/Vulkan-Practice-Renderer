#include "game.h"

#include "game_rendering.h"
#include "player_controller.h"
#include "containers/darray.h"
#include "core/logger.h"
#include "core/input.h"
#include "marching_cubes.h"
#include "math/lin_alg.h"



GameState* gameState = nullptr;



void GameInit()
{
    gameState = Alloc(GetGlobalAllocator(), sizeof(*gameState), MEM_TAG_GAME);

	GameRenderingInit();
	PlayerControllerInit();

    StartOrResetTimer(&gameState->timer);
}

void GameShutdown()
{
	PlayerControllerShutdown();
	GameRenderingShutdown();

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

	PlayerControllerUpdate();
	GameRenderingRender();
}
