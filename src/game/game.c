#include "game.h"

#include "containers/darray.h"
#include "core/engine.h"
#include "core/input.h"
#include "core/logger.h"
#include "game_rendering.h"
#include "marching_cubes.h"
#include "math/lin_alg.h"
#include "player_controller.h"

int main()
{
    // ================================================================== Startup
    EngineInit();

    GameRenderingInit();
    PlayerControllerInit();

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
    }

    // ================================================================== Shutdown
    PlayerControllerShutdown();
    GameRenderingShutdown();

    EngineShutdown();
}
