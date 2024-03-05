#include "input.h"

#include "core/asserts.h"
#include "core/meminc.h"
#include "core/event.h"
#include "core/platform.h"


typedef struct InputState
{
	bool keyStates[256];
	bool previousKeyStates[256];
	bool buttonStates[7];
	bool previousButtonStates[7];
	i32 mousePosX;
	i32 mousePosY;
	i32 previousMousePosX;
	i32 previousMousePosY;
	bool mouseCentered;
} InputState;

static InputState* state = nullptr;


bool InitializeInput()
{
	GRASSERT_DEBUG(state == nullptr); // If this fails it means init was called twice
	_INFO("Initializing input subsystem...");

	state = Alloc(GetGlobalAllocator(), sizeof(InputState), MEM_TAG_INPUT_SUBSYS);
	MemoryZero(state, sizeof(InputState));

	return true;
}

void ShutdownInput()
{
	if (state == nullptr)
	{
		_INFO("Input startup failed, skipping shutdown");
		return;
	}
	else
	{
		_INFO("Shutting down input subsystem...");
	}

	Free(GetGlobalAllocator(), state);
}

void UpdateInput()
{
	MemoryCopy(&state->previousKeyStates, &state->keyStates, sizeof(state->keyStates));
	MemoryCopy(&state->previousButtonStates, &state->buttonStates, sizeof(state->buttonStates));
	state->previousMousePosX = state->mousePosX;
	state->previousMousePosY = state->mousePosY;
	if (state->mouseCentered)
	{
		vec2i centerOfScreen = { .x = GetPlatformWindowSize().x / 2, .y = GetPlatformWindowSize().y / 2 };
		SetMousePosition(centerOfScreen);
	}
}

void InputSetMouseCentered(bool enabled)
{
	state->mouseCentered = enabled;
}

void InputToggleMouseCentered()
{
	state->mouseCentered = !state->mouseCentered;
}

bool GetKeyDown(KeyCode key)
{
	return state->keyStates[key];
}

bool GetKeyDownPrevious(KeyCode key)
{
	return state->previousKeyStates[key];
}

bool GetButtonDown(ButtonCode button)
{
	return state->buttonStates[button];
}

bool GetButtonDownPrevious(ButtonCode button)
{
	return state->previousButtonStates[button];
}

vec2i GetMousePos()
{
	vec2i mousePos = { state->mousePosX, state->mousePosY };
	return mousePos;
}

vec2i GetMousePosPrevious()
{
	vec2i previousMousePos = { state->previousMousePosX, state->previousMousePosY };
	return previousMousePos;
}

vec2i GetMouseDistanceFromCenter()
{
	vec2i mouseDistanceFromCenter = {state->mousePosX - (GetPlatformWindowSize().x / 2), state->mousePosY - (GetPlatformWindowSize().y / 2)};
	return mouseDistanceFromCenter;
}

void ProcessKey(bool down, KeyCode key)
{
	if (state->keyStates[key] != down)
	{
		state->keyStates[key] = down;
		EventData data = {};
		data.u8[0] = key;
		if (down)
			InvokeEvent(EVCODE_KEY_DOWN, data);
		else
			InvokeEvent(EVCODE_KEY_UP, data);
	}
}

void ProcessButton(bool down, ButtonCode button)
{
	if (state->buttonStates[button] != down)
	{
		state->buttonStates[button] = down;
		EventData data = {};
		data.u8[0] = button;
		if (down)
			InvokeEvent(EVCODE_BUTTON_DOWN, data);
		else
			InvokeEvent(EVCODE_BUTTON_UP, data);
	}
}

void ProcessMouseMove(i32 x, i32 y)
{
	if (state->mousePosX != x || state->mousePosY != y)
	{
		state->mousePosX = x;
		state->mousePosY = y;
		EventData data = {};
		data.i32[0] = state->mousePosX;
		data.i32[1] = state->mousePosY;
		data.i32[2] = state->previousMousePosX;
		data.i32[3] = state->previousMousePosY;
		InvokeEvent(EVCODE_MOUSE_MOVED, data);
	}
}
