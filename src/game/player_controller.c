#include "player_controller.h"

#include "core/input.h"
#include "game_rendering.h"
#include "renderer/camera.h"
#include "core/engine.h"

typedef struct ControllerState
{
    Camera* sceneCamera; // Scene camera retrieved from game rendering, this doesn't have ownership
    DebugMenu* controllerSettingMenu;
    f32 mouseSensitivity;
    f32 movementSpeed;
    bool cameraControlActive; // Whether the player can currently control the camera or not.
    bool controlCameraButtonPressed;
} ControllerState;

static ControllerState* controllerState = nullptr;

void PlayerControllerInit()
{
    controllerState = Alloc(GetGlobalAllocator(), sizeof(*controllerState), MEM_TAG_TEST);

    controllerState->sceneCamera = GetGameCameras().sceneCamera;
    controllerState->mouseSensitivity = 0.5f;
    controllerState->movementSpeed = 300.f;
    controllerState->cameraControlActive = true;
    controllerState->controlCameraButtonPressed = false;
    InputSetMouseCentered(controllerState->cameraControlActive);

    // Creating debug menu
    controllerState->controllerSettingMenu = DebugUICreateMenu();
    RegisterDebugMenu(controllerState->controllerSettingMenu);
    DebugUIAddButton(controllerState->controllerSettingMenu, "control camera", nullptr, &controllerState->controlCameraButtonPressed);
    DebugUIAddSliderLog(controllerState->controllerSettingMenu, "mouse sensitivity", 10.f, 0.0001f, 0.01f, &controllerState->mouseSensitivity);
    DebugUIAddSliderLog(controllerState->controllerSettingMenu, "move speed", 10.f, 1.f, 1000.f, &controllerState->movementSpeed);
}

void PlayerControllerUpdate()
{
    Camera* sceneCamera = controllerState->sceneCamera;

    // Calculating the player movement and camera movement, if camera control is active
    if (controllerState->cameraControlActive)
    {
		vec3 frameRotation = vec3_mul_f32(vec3_create(GetMouseDistanceFromCenter().y, GetMouseDistanceFromCenter().x, 0), controllerState->mouseSensitivity);
		sceneCamera->rotation = vec3_add_vec3(sceneCamera->rotation, frameRotation);

        if (sceneCamera->rotation.x > 1.5f)
            sceneCamera->rotation.x = 1.5f;
        if (sceneCamera->rotation.x < -1.5f)
            sceneCamera->rotation.x = -1.5f;

        // Create the rotation matrix
        mat4 rotation = mat4_rotate_xyz(vec3_invert_sign(sceneCamera->rotation));

        vec3 forwardVector = {-rotation.values[2 + COL4(0)], -rotation.values[2 + COL4(1)], -rotation.values[2 + COL4(2)]};
        vec3 rightVector = {rotation.values[0 + COL4(0)], rotation.values[0 + COL4(1)], rotation.values[0 + COL4(2)]};

        vec3 frameMovement = {};

        if (GetKeyDown(KEY_A))
            frameMovement = vec3_sub_vec3(frameMovement, rightVector);
        if (GetKeyDown(KEY_D))
            frameMovement = vec3_add_vec3(frameMovement, rightVector);
        if (GetKeyDown(KEY_S))
            frameMovement = vec3_sub_vec3(frameMovement, forwardVector);
        if (GetKeyDown(KEY_W))
            frameMovement = vec3_add_vec3(frameMovement, forwardVector);
        if (GetKeyDown(KEY_SHIFT))
            frameMovement.y -= 1;
        if (GetKeyDown(KEY_SPACE))
            frameMovement.y += 1;
        sceneCamera->position = vec3_add_vec3(sceneCamera->position, vec3_mul_f32(frameMovement, controllerState->movementSpeed * grGlobals->deltaTime));
    }

    // If the control camera button is pressed or if the camera control is enabled and the player presses LMB.
    if ((controllerState->controlCameraButtonPressed) ||
        (controllerState->cameraControlActive && GetButtonDown(BUTTON_LEFTMOUSEBTN) && !GetButtonDownPrevious(BUTTON_LEFTMOUSEBTN)))
    {
		controllerState->cameraControlActive = !controllerState->cameraControlActive;
        InputSetMouseCentered(controllerState->cameraControlActive);
        controllerState->controlCameraButtonPressed = false;
    }
}

void PlayerControllerShutdown()
{
    UnregisterDebugMenu(controllerState->controllerSettingMenu);
    DebugUIDestroyMenu(controllerState->controllerSettingMenu);

    Free(GetGlobalAllocator(), controllerState);
}
