#include "game.h"

#include "containers/darray.h"
#include "core/engine.h"
#include "core/input.h"
#include "core/logger.h"
#include "game_rendering.h"
#include "marching_cubes.h"
#include "math/lin_alg.h"
#include "player_controller.h"

GameState* gameState = nullptr;

int main()
{
    // ================================================================== Startup
    EngineInit();

    gameState = Alloc(GetGlobalAllocator(), sizeof(*gameState), MEM_TAG_GAME);

    gameState->frameArena = ArenaCreate(GetGlobalAllocator(), MiB * 50);
    GameRenderingInit();
    PlayerControllerInit();

    ArenaClear(&gameState->frameArena);

    // ================================================================= Game loop
    while (EngineUpdate())
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

    // ================================================================== Shutdown
    PlayerControllerShutdown();
    GameRenderingShutdown();

    ArenaClear(&gameState->frameArena);
    ArenaDestroy(&gameState->frameArena, GetGlobalAllocator());
    Free(GetGlobalAllocator(), gameState);

    EngineShutdown();
}
