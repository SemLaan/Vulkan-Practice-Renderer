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
	EngineInitSettings engineSettings = {};
	engineSettings.presentMode = GR_PRESENT_MODE_FIFO;
	engineSettings.startResolution = (vec2i){ .x = 200, .y = 200};
	engineSettings.windowTitle = "Beefbal Beefer 44";
	engineSettings.framerateLimit = 14000;
    EngineInit(engineSettings);

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
