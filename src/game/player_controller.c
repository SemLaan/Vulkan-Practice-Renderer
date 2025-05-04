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
	f32 arcballRadius;
	Camera freeCameraState;
	Camera arcballCameraState;
    bool cameraControlActive; // Whether the player can currently control the camera or not.
	bool controllingArcball;
    bool controlCameraButtonPressed;
	bool controlArcballCameraButtonPressed;
} ControllerState;

static ControllerState* controllerState = nullptr;

void PlayerControllerInit()
{
    controllerState = Alloc(GetGlobalAllocator(), sizeof(*controllerState));

    controllerState->sceneCamera = GetGameCameras().sceneCamera;
    controllerState->mouseSensitivity = 0.5f;
    controllerState->movementSpeed = 300.f;
	controllerState->controllingArcball = false;
    controllerState->cameraControlActive = true;
    controllerState->controlCameraButtonPressed = false;
	controllerState->controlArcballCameraButtonPressed = false;
    InputSetMouseCentered(controllerState->cameraControlActive);

    // Creating debug menu
    controllerState->controllerSettingMenu = DebugUICreateMenu("Mouse Settings");
    DebugUIAddButton(controllerState->controllerSettingMenu, "control camera", nullptr, &controllerState->controlCameraButtonPressed);
    DebugUIAddButton(controllerState->controllerSettingMenu, "control arcball camera", nullptr, &controllerState->controlArcballCameraButtonPressed);
    DebugUIAddSliderLog(controllerState->controllerSettingMenu, "mouse sensitivity", 10.f, 0.0001f, 0.01f, &controllerState->mouseSensitivity);
    DebugUIAddSliderLog(controllerState->controllerSettingMenu, "move speed", 10.f, 1.f, 1000.f, &controllerState->movementSpeed);
	DebugUIAddSliderFloat(controllerState->controllerSettingMenu, "Arcball Radius", 10, 100, &controllerState->arcballRadius);

	controllerState->freeCameraState.position = vec3_create(0, 0, 0);
	controllerState->freeCameraState.rotation = vec3_create(0, 0, 0);
	controllerState->arcballCameraState.position = vec3_create(0, 0, 0);
	controllerState->arcballCameraState.rotation = vec3_create(0, 0, 0);
}

void PlayerControllerUpdate()
{
    Camera* sceneCamera = controllerState->sceneCamera;

	// If the control camera button is pressed or if the camera control is enabled and the player presses LMB.
    if (controllerState->controlCameraButtonPressed)
    {
        controllerState->controlCameraButtonPressed = false;
		controllerState->controllingArcball = false;
		controllerState->cameraControlActive = true;
        InputSetMouseCentered(true);
		sceneCamera->position = controllerState->freeCameraState.position;
		sceneCamera->rotation = controllerState->freeCameraState.rotation;
    }

	if (controllerState->controlArcballCameraButtonPressed)
	{
		controllerState->controlArcballCameraButtonPressed = false;
		controllerState->cameraControlActive = true;
		controllerState->controllingArcball = true;
		InputSetMouseCentered(true);
		sceneCamera->position = controllerState->arcballCameraState.position;
		sceneCamera->rotation = controllerState->arcballCameraState.rotation;
	}

	if (controllerState->cameraControlActive && GetButtonDown(BUTTON_LEFTMOUSEBTN) && !GetButtonDownPrevious(BUTTON_LEFTMOUSEBTN))
	{
		InputSetMouseCentered(false);
		controllerState->cameraControlActive = false;
		if (controllerState->controllingArcball)
			controllerState->arcballCameraState = *sceneCamera;
		else
			controllerState->freeCameraState = *sceneCamera;
	}

    // Calculating the player movement and camera movement, if camera control is active
    if (controllerState->cameraControlActive && !controllerState->controllingArcball)
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
        sceneCamera->position = vec3_add_vec3(sceneCamera->position, vec3_mul_f32(frameMovement, controllerState->movementSpeed * global->deltaTime));
    }
	// Camera movement in case arcball control is enabled
	else if (controllerState->cameraControlActive && controllerState->controllingArcball)
	{
		vec3 frameRotation = vec3_mul_f32(vec3_create(GetMouseDistanceFromCenter().y, GetMouseDistanceFromCenter().x, 0), controllerState->mouseSensitivity);
		sceneCamera->rotation = vec3_add_vec3(sceneCamera->rotation, frameRotation);

        if (sceneCamera->rotation.x > 1.5f)
            sceneCamera->rotation.x = 1.5f;
        if (sceneCamera->rotation.x < -1.5f)
            sceneCamera->rotation.x = -1.5f;
	}

	if (controllerState->controllingArcball)
	{
		// Create the rotation matrix
        mat4 rotation = mat4_rotate_xyz(vec3_invert_sign(sceneCamera->rotation));

        vec3 forwardVector = {-rotation.values[2 + COL4(0)], -rotation.values[2 + COL4(1)], -rotation.values[2 + COL4(2)]};

		sceneCamera->position = vec3_mul_f32(forwardVector, -controllerState->arcballRadius);
	}
}

void PlayerControllerShutdown()
{
    DebugUIDestroyMenu(controllerState->controllerSettingMenu);

    Free(GetGlobalAllocator(), controllerState);
}
