// This code only implements platform.h if the platform is windows
// Otherwise this entire script does nothing
#ifdef __win__
// Both of these headers are implemented here
#include "platform.h"
#include "renderer/vulkan_renderer/vulkan_platform.h" 

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <vulkan/vulkan_win32.h>
#include "core/logger.h"
#include "core/asserts.h"
#include "core/meminc.h"
#include "core/event.h"
#include "core/input.h"

// TODO: change all windows functions to their "A" version

typedef struct PlatformState
{
	f64 clockFrequency;
	HWND hwnd;
	u32 width;
	u32 height;
	u32 xPosPreMaximize, yPosPreMaximize;
	u32 widthPreMaximize, heightPreMaximize;
	DWORD windowStyle;
	DWORD windowExStyle;
	bool fullscreenKeyDown;
	bool fullscreenActive;
} PlatformState;

static PlatformState* state = nullptr;

// Forward declaring window callbacks
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

bool InitializePlatform(const char* windowName, u32 windowWidth, u32 windowHeight) // TODO: take in window width, height and use it
{
	GRASSERT_DEBUG(state == nullptr); // If this fails init platform was called twice
	_INFO("Initializing platform subsystem...");
	state = Alloc(GetGlobalAllocator(), sizeof(PlatformState));

	LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    state->clockFrequency = 1.0 / (f64)frequency.QuadPart;

	const char* menuName = "gorilwinmenu";
	const char* className = "gorilwinclass";

	WNDCLASSEX wc = {};

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = WindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = GetModuleHandle(NULL);
	wc.hIcon = LoadIcon(NULL, IDC_ICON);
	wc.hIconSm = NULL;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = menuName;
	wc.lpszClassName = className;

	RegisterClassEx(&wc);

	state->windowStyle = WS_OVERLAPPEDWINDOW;
	state->windowExStyle = WS_EX_OVERLAPPEDWINDOW | WS_EX_APPWINDOW;

    RECT realWindowSize = { 0, 0, windowWidth, windowHeight};
    AdjustWindowRectEx(&realWindowSize, state->windowStyle, 0, state->windowExStyle);

	state->hwnd = CreateWindowExA(
		state->windowExStyle, className, windowName,
		state->windowStyle,
		600, 300, realWindowSize.right - realWindowSize.left, realWindowSize.bottom - realWindowSize.top,
		NULL, NULL, GetModuleHandle(NULL), NULL
	);

	if (state->hwnd == NULL)
	{
		_FATAL("Creating window failed");
		Free(GetGlobalAllocator(), state);
		state = nullptr;
		return false;
	}

	ShowWindow(state->hwnd, SW_SHOW);

	RECT clientRect;
	if (GetClientRect(state->hwnd, &clientRect))
	{
		state->width = clientRect.right - clientRect.left;
		state->height = clientRect.bottom - clientRect.top;
	}

	return true;
}

void ShutdownPlatform()
{
	if (state == nullptr)
	{
		_INFO("Platform startup failed, skipping shutdown");
		return;
	}
	else
	{
		_INFO("Shutting down platform subsystem...");
	}

	Free(GetGlobalAllocator(), state);
}

void PlatformProcessMessage()
{
	MSG msg = {};

	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

static u8 logLevelColors[MAX_LOG_LEVELS] =
{
	192,
	4,
	6,
	9,
	2,
	8
};

void PlatformLogString(log_level level, const char* message)
{
	static HANDLE hConsole;
	if (!hConsole)
	{
		hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		_INFO("Console initialized");
	}
	SetConsoleTextAttribute(hConsole, logLevelColors[level]);
	OutputDebugStringA(message);
}

vec2i GetPlatformWindowSize()
{
	vec2i windowSize = { (i32)state->width, (i32)state->height };
	return windowSize;
}

void SetMousePosition(vec2i position)
{
	if (state->fullscreenActive)
	{
		SetCursorPos(position.x, position.y);
	}
	else
	{
		RECT windowRect, clientRect;
		if (!GetWindowRect(state->hwnd, &windowRect) || !GetClientRect(state->hwnd, &clientRect))
		{
			GRASSERT(false);
		}
		i32 leftBorderSize = ((windowRect.right - windowRect.left) - clientRect.right) / 2;
		SetCursorPos(windowRect.left + leftBorderSize + position.x, windowRect.top + GetSystemMetrics(SM_CYCAPTION) + leftBorderSize + position.y);
	}
}

void SetWindowTitle(const char* windowName)
{
	SetWindowText(state->hwnd, windowName);
}

void ToggleFullscreen()
{
	SetFullscreen(!state->fullscreenActive);
}

void SetFullscreen(bool enabled)
{
	if (enabled == state->fullscreenActive)
		return;

	state->fullscreenActive = enabled;

	if (enabled)
	{
		SetWindowLong(state->hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
		SetWindowLong(state->hwnd, GWL_EXSTYLE, WS_EX_APPWINDOW);

		HMONITOR monitor = MonitorFromWindow(state->hwnd, MONITOR_DEFAULTTONEAREST);
		MONITORINFO monitorInfo = {};
		monitorInfo.cbSize = sizeof(monitorInfo);
		if (GetMonitorInfo(monitor, &monitorInfo))
		{
			RECT windowRect;
			if (GetWindowRect(state->hwnd, &windowRect))
			{
				state->xPosPreMaximize = windowRect.left;
				state->yPosPreMaximize = windowRect.top;
			}
			state->widthPreMaximize = state->width;
			state->heightPreMaximize = state->height;
			state->width = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
			state->height = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;

			SetWindowPos(state->hwnd, nullptr,
				monitorInfo.rcMonitor.left,
				monitorInfo.rcMonitor.top,
				monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
				monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
				SWP_NOZORDER
			);

			EventData eventData = {};
			eventData.u32[0] = state->width;
			eventData.u32[1] = state->height;
			InvokeEvent(EVCODE_WINDOW_RESIZED, eventData);
		}
	}
	else
	{
		SetWindowLongW(state->hwnd, GWL_STYLE, state->windowStyle);
		SetWindowLongW(state->hwnd, GWL_EXSTYLE, state->windowExStyle);

		ShowWindow(state->hwnd, SW_SHOW);

		SetWindowPos(state->hwnd, nullptr,
			state->xPosPreMaximize,
			state->yPosPreMaximize,
			state->widthPreMaximize,
			state->heightPreMaximize,
			0
		);

		RECT clientRect;
		if (GetClientRect(state->hwnd, &clientRect))
		{
			state->width = clientRect.right - clientRect.left;
			state->height = clientRect.bottom - clientRect.top;
		}

		EventData eventData = {};
		eventData.u32[0] = state->width;
		eventData.u32[1] = state->height;
		InvokeEvent(EVCODE_WINDOW_RESIZED, eventData);
	}
}

f64 PlatformGetTime()
{
	// Time since system boot in seconds
	LARGE_INTEGER now_time;
    QueryPerformanceCounter(&now_time);
    return (f64)now_time.QuadPart * state->clockFrequency;
}

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CLOSE:
	{
		EventData data = {};
		InvokeEvent(EVCODE_QUIT, data);
		break;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
		// ======= Input processing ===============
	case WM_MOUSEMOVE:
	{
		i32 xPos = GET_X_LPARAM(lParam);
		i32 yPos = GET_Y_LPARAM(lParam);
		ProcessMouseMove(xPos, yPos);
		break;
	}
	case WM_LBUTTONDOWN:
		ProcessButton(true, BUTTON_LEFTMOUSEBTN);
		break;
	case WM_LBUTTONUP:
		ProcessButton(false, BUTTON_LEFTMOUSEBTN);
		break;
	case WM_MBUTTONDOWN:
		ProcessButton(true, BUTTON_MIDMOUSEBTN);
		break;
	case WM_MBUTTONUP:
		ProcessButton(false, BUTTON_MIDMOUSEBTN);
		break;
	case WM_RBUTTONDOWN:
		ProcessButton(true, BUTTON_RIGHTMOUSEBTN);
		break;
	case WM_RBUTTONUP:
		ProcessButton(false, BUTTON_RIGHTMOUSEBTN);
		break;
	case WM_KEYDOWN:
		if (wParam == 0x12)
			break;
		ProcessKey(true, (KeyCode)wParam);
		break;
	case WM_KEYUP:
		if (wParam == 0x12)
			break;
		ProcessKey(false, (KeyCode)wParam);
		break;
		/// TODO: process syskeys for input, if you want to suffer
	case WM_SIZE:
	{
		RECT clientRect;
		u32 width, height;
		if (GetClientRect(hwnd, &clientRect))
		{
			width = clientRect.right - clientRect.left;
			height = clientRect.bottom - clientRect.top;
			if (width != state->width || height != state->height)
			{
				_DEBUG("Window resized");
				state->width = width;
				state->height = height;
				EventData eventData = {};
				eventData.u32[0] = state->width;
				eventData.u32[1] = state->height;
				InvokeEvent(EVCODE_WINDOW_RESIZED, eventData);
				break;
			}
		}
	}
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// ============ Vulkan platform implementation =======================
void GetPlatformExtensions(u32* pExtensionNameCount, const char** extensionNames)
{
	extensionNames[*pExtensionNameCount] = "VK_KHR_win32_surface";
	*pExtensionNameCount += 1;
}

bool PlatformCreateSurface(VkInstance instance, VkAllocationCallbacks* allocator, VkSurfaceKHR* out_surface)
{
	VkWin32SurfaceCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.hinstance = GetModuleHandle(NULL);
	createInfo.hwnd = state->hwnd;

	if (VK_SUCCESS != vkCreateWin32SurfaceKHR(instance, &createInfo, allocator, out_surface))
	{
		memset(out_surface, 0, sizeof(VkSurfaceKHR));
		return false;
	}

	return true;
}

#endif