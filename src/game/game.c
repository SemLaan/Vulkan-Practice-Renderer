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

	gameState->frameArena = ArenaCreate(GetGlobalAllocator(), MiB * 5);
	GameRenderingInit();
	PlayerControllerInit();

	ArenaClear(&gameState->frameArena);

    StartOrResetTimer(&gameState->timer);
}

void GameShutdown()
{
	PlayerControllerShutdown();
	GameRenderingShutdown();

	ArenaClear(&gameState->frameArena);
	ArenaDestroy(&gameState->frameArena, GetGlobalAllocator());
    Free(GetGlobalAllocator(), gameState);
}

void GameUpdateAndRender()
{
    // =========================== Update ===================================
	if (GetButtonDown(BUTTON_RIGHTMOUSEBTN) && !GetButtonDownPrevious(BUTTON_RIGHTMOUSEBTN))
	{
		RegenerateMarchingCubesMesh();
	}

	PlayerControllerUpdate();
	GameRenderingRender();

	ArenaClear(&gameState->frameArena);
}
