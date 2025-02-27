#pragma once
#include "defines.h"
#include "math/math_types.h"

typedef enum ButtonCode
{
	BUTTON_LEFTMOUSEBTN = 0x01, //Left mouse button
	BUTTON_RIGHTMOUSEBTN = 0x02, //Right mouse button
	//BUTTON_CTRLBRKPRCS = 0x03, //Control-break processing
	BUTTON_MIDMOUSEBTN = 0x04, //Middle mouse button
	//BUTTON_THUMBFORWARD = 0x05, //Thumb button back on mouse aka X1
	//BUTTON_THUMBBACK = 0x06, //Thumb button forward on mouse aka X2
} ButtonCode;

typedef enum KeyCode
{
	KEY_BACKSPACE = 0x08, //Backspace key
	KEY_TAB = 0x09, //Tab key
	KEY_ENTER = 0x0D, //Enter or Return key
	KEY_SHIFT = 0x10, //Shift key
	KEY_CONTROL = 0x11, //Ctrl key
	KEY_ALT = 0x12, //Alt key

	KEY_PAUSE = 0x13, //Pause key
	KEY_CAPSLOCK = 0x14, //Caps lock key
	KEY_ESCAPE = 0x1B, //Esc key
	KEY_LEFTWIN = 0x5B, //Left windows key
	KEY_RIGHTWIN = 0x5C, //Right windows key

	///TODO: none of these are implemented yet
	KEY_LEFTSHIFT = 0xA0, //Left shift key
	KEY_RIGHTSHIFT = 0xA1, //Right shift key
	KEY_LEFTCTRL = 0xA2, //Left control key
	KEY_RIGHTCTRL = 0xA3, //Right control key
	KEY_LEFTMENU = 0xA4, //Left menu key
	KEY_RIGHTMENU = 0xA5, //Right menu

	KEY_PLUS = 0xBB, //Plus key
	KEY_COMMA = 0xBC, //Comma key
	KEY_MINUS = 0xBD, //Minus key
	KEY_PERIOD = 0xBE, //Period key

	KEY_Num0 = 0x30, //Top row 0 key (Matches '0')
	KEY_Num1 = 0x31, //Top row 1 key (Matches '1')
	KEY_Num2 = 0x32, //Top row 2 key (Matches '2')
	KEY_Num3 = 0x33, //Top row 3 key (Matches '3')
	KEY_Num4 = 0x34, //Top row 4 key (Matches '4')
	KEY_Num5 = 0x35, //Top row 5 key (Matches '5')
	KEY_Num6 = 0x36, //Top row 6 key (Matches '6')
	KEY_Num7 = 0x37, //Top row 7 key (Matches '7')
	KEY_Num8 = 0x38, //Top row 8 key (Matches '8')
	KEY_Num9 = 0x39, //Top row 9 key (Matches '9')
	KEY_A = 0x41, //A key (Matches 'A')
	KEY_B = 0x42, //B key (Matches 'B')
	KEY_C = 0x43, //C key (Matches 'C')
	KEY_D = 0x44, //D key (Matches 'D')
	KEY_E = 0x45, //E key (Matches 'E')
	KEY_F = 0x46, //F key (Matches 'F')
	KEY_G = 0x47, //G key (Matches 'G')
	KEY_H = 0x48, //H key (Matches 'H')
	KEY_I = 0x49, //I key (Matches 'I')
	KEY_J = 0x4A, //J key (Matches 'J')
	KEY_K = 0x4B, //K key (Matches 'K')
	KEY_L = 0x4C, //L key (Matches 'L')
	KEY_M = 0x4D, //M key (Matches 'M')
	KEY_N = 0x4E, //N key (Matches 'N')
	KEY_O = 0x4F, //O key (Matches 'O')
	KEY_P = 0x50, //P key (Matches 'P')
	KEY_Q = 0x51, //Q key (Matches 'Q')
	KEY_R = 0x52, //R key (Matches 'R')
	KEY_S = 0x53, //S key (Matches 'S')
	KEY_T = 0x54, //T key (Matches 'T')
	KEY_U = 0x55, //U key (Matches 'U')
	KEY_V = 0x56, //V key (Matches 'V')
	KEY_W = 0x57, //W key (Matches 'W')
	KEY_X = 0x58, //X key (Matches 'X')
	KEY_Y = 0x59, //Y key (Matches 'Y')
	KEY_Z = 0x5A, //Z key (Matches 'Z')
	KEY_NUMPAD0 = 0x60, //Numpad 0
	KEY_NUMPAD1 = 0x61, //Numpad 1
	KEY_NUMPAD2 = 0x62, //Numpad 2
	KEY_NUMPAD3 = 0x63, //Numpad 3
	KEY_NUMPAD4 = 0x64, //Numpad 4
	KEY_NUMPAD5 = 0x65, //Numpad 5
	KEY_NUMPAD6 = 0x66, //Numpad 6
	KEY_NUMPAD7 = 0x67, //Numpad 7
	KEY_NUMPAD8 = 0x68, //Numpad 8
	KEY_NUMPAD9 = 0x69, //Numpad 9
	KEY_MULTIPLY = 0x6A, //Multiply key
	KEY_ADD = 0x6B, //Add key
	KEY_SEPARATOR = 0x6C, //Separator key
	KEY_SUBTRACT = 0x6D, //Subtract key
	KEY_DECIMAL = 0x6E, //Decimal key
	KEY_DIVIDE = 0x6F, //Divide key
	KEY_F1 = 0x70, //F1
	KEY_F2 = 0x71, //F2
	KEY_F3 = 0x72, //F3
	KEY_F4 = 0x73, //F4
	KEY_F5 = 0x74, //F5
	KEY_F6 = 0x75, //F6
	KEY_F7 = 0x76, //F7
	KEY_F8 = 0x77, //F8
	KEY_F9 = 0x78, //F9
	KEY_F10 = 0x79, //F10
	KEY_F11 = 0x7A, //F11
	KEY_F12 = 0x7B, //F12
	KEY_F13 = 0x7C, //F13
	KEY_F14 = 0x7D, //F14
	KEY_F15 = 0x7E, //F15
	KEY_F16 = 0x7F, //F16
	KEY_F17 = 0x80, //F17
	KEY_F18 = 0x81, //F18
	KEY_F19 = 0x82, //F19
	KEY_F20 = 0x83, //F20
	KEY_F21 = 0x84, //F21
	KEY_F22 = 0x85, //F22
	KEY_F23 = 0x86, //F23
	KEY_F24 = 0x87, //F24
	KEY_SPACE = 0x20, //Space bar
	//0x07 : reserved
	//0x0A - 0x0B : reserved
	//0x0E - 0x0F : unassigned
	//KEY_Kana = 0x15, //Kana input mode
	//KEY_Hangeul = 0x15, //Hangeul input mode
	//KEY_Hangul = 0x15, //Hangul input mode
	//0x16 : unassigned
	//KEY_Junju = 0x17, //Junja input method
	//KEY_Final = 0x18, //Final input method
	//KEY_Hanja = 0x19, //Hanja input method
	//KEY_Kanji = 0x19, //Kanji input method
	//0x1A : unassigned
	//KEY_Convert = 0x1C, //IME convert
	//KEY_NonConvert = 0x1D, //IME Non convert
	//KEY_Accept = 0x1E, //IME accept
	//KEY_ModeChange = 0x1F, //IME mode change
	//KEY_PageUp = 0x21, //Page up key
	//KEY_PageDown = 0x22, //Page down key
	//KEY_End = 0x23, //End key
	//KEY_Home = 0x24, //Home key
	//KEY_LeftArrow = 0x25, //Left arrow key
	//KEY_UpArrow = 0x26, //Up arrow key
	//KEY_RightArrow = 0x27, //Right arrow key
	//KEY_DownArrow = 0x28, //Down arrow key
	//KEY_Select = 0x29, //Select key
	//KEY_Print = 0x2A, //Print key
	//KEY_Execute = 0x2B, //Execute key
	//KEY_PrintScreen = 0x2C, //Print screen key
	//KEY_Inser = 0x2D, //Insert key
	//KEY_Delete = 0x2E, //Delete key
	//KEY_Help = 0x2F, //Help key
	//0x3A - 0x40 : unassigned
	//0x5E : reserved
	//0x88 - 0x8F : UI navigation
	//KEY_NavigationView = 0x88, //reserved
	//KEY_NavigationMenu = 0x89, //reserved
	//KEY_NavigationUp = 0x8A, //reserved
	//KEY_NavigationDown = 0x8B, //reserved
	//KEY_NavigationLeft = 0x8C, //reserved
	//KEY_NavigationRight = 0x8D, //reserved
	//KEY_NavigationAccept = 0x8E, //reserved
	//KEY_NavigationCancel = 0x8F, //reserved
	//KEY_NumLock = 0x90, //Num lock key
	//KEY_ScrollLock = 0x91, //Scroll lock key
	//KEY_FJ_Jisho = 0x92, //Dictionary key
	//KEY_FJ_Masshou = 0x93, //Unregister word key
	//KEY_FJ_Touroku = 0x94, //Register word key
	//KEY_FJ_Loya = 0x95, //Left OYAYUBI key
	//KEY_FJ_Roya = 0x96, //Right OYAYUBI key
	//0x97 - 0x9F : unassigned
	//KEY_BrowserBack = 0xA6, //Browser back button
	//KEY_BrowserForward = 0xA7, //Browser forward button
	//KEY_BrowserRefresh = 0xA8, //Browser refresh button
	//KEY_BrowserStop = 0xA9, //Browser stop button
	//KEY_BrowserSearch = 0xAA, //Browser search button
	//KEY_BrowserFavorites = 0xAB, //Browser favorites button
	//KEY_BrowserHome = 0xAC, //Browser home button
	//KEY_VolumeMute = 0xAD, //Volume mute button
	//KEY_VolumeDown = 0xAE, //Volume down button
	//KEY_VolumeUp = 0xAF, //Volume up button
	//KEY_NextTrack = 0xB0, //Next track media button
	//KEY_PrevTrack = 0xB1, //Previous track media button
	//KEY_Stop = 0xB2, //Stop media button
	//KEY_PlayPause = 0xB3, //Play/pause media button
	//KEY_Mail = 0xB4, //Launch mail button
	//KEY_MediaSelect = 0xB5, //Launch media select button
	//KEY_App1 = 0xB6, //Launch app 1 button
	//KEY_App2 = 0xB7, //Launch app 2 button
	//0xB8 - 0xB9 : reserved
	//KEY_OEM1 = 0xBA, //;: key for US or misc keys for others
	//KEY_OEM2 = 0xBF, //? for US or misc keys for others
	//KEY_OEM3 = 0xC0, //~ for US or misc keys for others
	//0xC1 - 0xC2 : reserved
	//KEY_Gamepad_A = 0xC3, //Gamepad A button
	//KEY_Gamepad_B = 0xC4, //Gamepad B button
	//KEY_Gamepad_X = 0xC5, //Gamepad X button
	//KEY_Gamepad_Y = 0xC6, //Gamepad Y button
	//KEY_GamepadRightBumper = 0xC7, //Gamepad right bumper
	//KEY_GamepadLeftBumper = 0xC8, //Gamepad left bumper
	//KEY_GamepadLeftTrigger = 0xC9, //Gamepad left trigger
	//KEY_GamepadRightTrigger = 0xCA, //Gamepad right trigger
	//KEY_GamepadDPadUp = 0xCB, //Gamepad DPad up
	//KEY_GamepadDPadDown = 0xCC, //Gamepad DPad down
	//KEY_GamepadDPadLeft = 0xCD, //Gamepad DPad left
	//KEY_GamepadDPadRight = 0xCE, //Gamepad DPad right
	//KEY_GamepadMenu = 0xCF, //Gamepad menu button
	//KEY_GamepadView = 0xD0, //Gamepad view button
	//KEY_GamepadLeftStickBtn = 0xD1, //Gamepad left stick button
	//KEY_GamepadRightStickBtn = 0xD2, //Gamepad right stick button
	//KEY_GamepadLeftStickUp = 0xD3, //Gamepad left stick up
	//KEY_GamepadLeftStickDown = 0xD4, //Gamepad left stick down
	//KEY_GamepadLeftStickRight = 0xD5, //Gamepad left stick right
	//KEY_GamepadLeftStickLeft = 0xD6, //Gamepad left stick left
	//KEY_GamepadRightStickUp = 0xD7, //Gamepad right stick up
	//KEY_GamepadRightStickDown = 0xD8, //Gamepad right stick down
	//KEY_GamepadRightStickRight = 0xD9, //Gamepad right stick right
	//KEY_GamepadRightStickLeft = 0xDA, //Gamepad right stick left
	//KEY_OEM4 = 0xDB, //[ for US or misc keys for others
	//KEY_OEM5 = 0xDC, //\ for US or misc keys for others
	//KEY_OEM6 = 0xDD, //] for US or misc keys for others
	//KEY_OEM7 = 0xDE, //' for US or misc keys for others
	//KEY_OEM8 = 0xDF, //Misc keys for others
	//0xE0 : reserved
	//KEY_OEMAX = 0xE1, //AX key on Japanese AX keyboard
	//KEY_OEM102 = 0xE2, //"<>" or "\|" on RT 102-key keyboard
	//KEY_ICOHelp = 0xE3, //Help key on ICO
	//KEY_ICO00 = 0xE4, //00 key on ICO
	//KEY_ProcessKey = 0xE5, //Process key input method
	//KEY_OEMCLEAR = 0xE6, //OEM specific
	//KEY_Packet = 0xE7, //IDK man try to google it
	//0xE8 : unassigned
	//KEY_OEMReset = 0xE9, //OEM reset button
	//KEY_OEMJump = 0xEA, //OEM jump button
	//KEY_OEMPA1 = 0xEB, //OEM PA1 button
	//KEY_OEMPA2 = 0xEC, //OEM PA2 button
	//KEY_OEMPA3 = 0xED, //OEM PA3 button
	//KEY_OEMWSCtrl = 0xEE, //OEM WS Control button
	//KEY_OEMCusel = 0xEF, //OEM CUSEL button
	//KEY_OEMAttn = 0xF0, //OEM ATTN button
	//KEY_OEMFinish = 0xF1, //OEM finish button
	//KEY_OEMCopy = 0xF2, //OEM copy button
	//KEY_OEMAuto = 0xF3, //OEM auto button
	//KEY_OEMEnlw = 0xF4, //OEM ENLW
	//KEY_OEMBackTab = 0xF5, //OEM back tab
	//KEY_Attn = 0xF6, //Attn
	//KEY_CrSel = 0xF7, //CrSel
	//KEY_ExSel = 0xF8, //ExSel
	//KEY_EraseEOF = 0xF9, //Erase EOF key
	//KEY_Play = 0xFA, //Play key
	//KEY_Zoom = 0xFB, //Zoom key
	//KEY_NoName = 0xFC, //No name
	//KEY_PA1 = 0xFD, //PA1 key
	//KEY_OEMClear = 0xFE, //OEM Clear key
} KeyCode;


bool InitializeInput();
void ShutdownInput();

// Copies the current input to the input of the previous frame, needs to happen before processing platform messages
void PreMessagesInputUpdate();
void PostMessagesInputUpdate();

// Will center the mouse every frame if set to true
void InputSetMouseCentered(bool enabled);
// Toggles whether the mouse is centered every frame
void InputToggleMouseCentered();

bool GetKeyDown(KeyCode key);
bool GetKeyDownPrevious(KeyCode key);
bool GetButtonDown(ButtonCode button);
bool GetButtonDownPrevious(ButtonCode button);
vec2i GetMousePos();
vec2i GetMousePosPrevious();
vec2i GetMouseDistanceFromCenter();

// Called by platform to inform a key was pressed
void ProcessKey(bool down, KeyCode key);
// Called by platform to inform a button was pressed
void ProcessButton(bool down, ButtonCode button);
// Called by platform to inform the mouse was moved
void ProcessMouseMove(i32 x, i32 y);
