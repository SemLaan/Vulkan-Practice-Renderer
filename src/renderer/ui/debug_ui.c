#include "debug_ui.h"

#include "core/asserts.h"
#include "core/event.h"
#include "core/input.h"
#include "core/meminc.h"
#include "core/platform.h"
#include "math/lin_alg.h"
#include "renderer/renderer.h"
#include "renderer/ui/text_renderer.h"

#define MAX_DBG_MENU_QUADS 100 // Change if you need more
#define MAX_DBG_MENU_INTERACTABLES 20 // Change if you need more
#define MAX_DBG_MENUS 3	// Change if you need more
#define ITERACTABLE_INTERNAL_DATA_ALLOCATOR_SIZE (KiB * 5) // This size is arbitrary :)
#define NO_INTERACTABLE_ACTIVE_VALUE -1


// ====================== Debug menu visual constant values =====================================
#define MENU_ORTHO_PROJECTION_HEIGHT 10
#define MENU_START_POSITION vec2_create(0.3f, 0.3f)
#define MENU_START_SIZE vec2_create(2.5f, 9.4f)
#define MENU_BACKGROUND_COLOR vec4_create(1, 1, 1, 1)
#define HANDLEBAR_VERTICAL_SIZE 0.5f
#define HANDLEBAR_COLOR vec4_create(0.2f, 0.2f, 0.8f, 1)
#define HANDLEBAR_PRESSED_COLOR vec4_create(0.1f, 0.1f, 0.7f, 1.0f)
#define MENU_ELEMENTS_OFFSET 0.3f
#define BUTTON_SIZE vec2_create(1, 1)
#define BUTTON_BASIC_COLOR vec4_create(1, 0, 0, 1)
#define BUTTON_PRESSED_COLOR vec4_create(0.0f, 0.0f, 1.0f, 1.0f)
#define SLIDER_BAR_SIZE vec2_create(2, 0.2f)
#define SLIDER_DOT_SIZE vec2_create(0.4f, 0.4f)
#define SLIDER_BAR_COLOR vec4_create(0.3f, 0.3f, 0.3f, 1)
#define SLIDER_DOT_COLOR vec4_create(1, 0, 0, 1)


typedef struct QuadInstanceData
{
    mat4 transform;
    vec4 color;
} QuadInstanceData;

typedef enum InteractableType
{
    INTERACTABLE_TYPE_BUTTON,
	INTERACTABLE_TYPE_MENU_HANDLEBAR,
	INTERACTABLE_TYPE_SLIDER,
    INTERACTABLE_TYPE_NONE
} InteractableType;

#define INTERACTABLE_TYPE_COUNT 3

// Interactable internal data for a button
typedef struct ButtonInteractableData
{
    bool* pStateBool;	// Pointer to a bool that will store whether the button is currently being held down or not.
    bool* pSignalBool;	// Pointer to a bool that will be set to true by the debug ui whenever the button is pressed (pressed = clicked on and let go by the player).
} ButtonInteractableData;

// Interactable internal data for the menu handlebar
typedef struct MenuHandlebarInteractableData
{
	vec4 mouseStartWorldPosition;	// Position of the mouse when it first clicked on the handlebar to move it.
	vec2 menuStartPosition;			// Position of the menu when the player first clicked on the handlebar to move it.
} MenuHandlebarInteractableData;

// Interactable internal data for the slider
typedef struct SliderInteractableData
{
	f32* pSliderValue;			// Pointer to a float that stores the value of the slider for the client to read out.
	f32 minValue;				// Minimum value of the slider.
	f32 maxValue;				// Maximum value of the slider.
	f32 valueRange;				// Max value - Min value. Used for calculating the position of the slider dot.
	f32 sliderDotHeight;		// y element of the slider dot position. Used for recalculating the dot transform after repositioning it.
} SliderInteractableData;


typedef struct InteractableData
{
    vec2 position;
    vec2 size;
    u32 firstQuad;
    u32 quadCount;
    InteractableType interactableType;
    void* internalData;
} InteractableData;

typedef struct DebugMenu
{
    mat4 menuTransform;                   // Transform of the menu.
    vec2 position;                        // Position, anchor is bottom left of the menu.
    vec2 size;                            // Vec 2 with the menu size, x is width y is height.
    InteractableData* interactablesArray; // Array of interactables (buttons, sliders, text, etc.).
    QuadInstanceData* quadsInstanceData;  // Instance data of the quads to render on the CPU.
    VertexBuffer quadsInstancedVB;        // Vertex buffer with model matrices for quads.
    Material menuElementMaterial;         // Material to render quads with.
    i32 activeInteractableIndex;          // Index into interactables array, is -1 if no interactable is being interacted with. (If a button is being pressed or a slider is being dragged, this will indicate that.)
    u32 maxQuads;                         // Max amount of quads
    u32 quadCount;                        // Current amount of quads
    u32 interactablesCount;               // Amount of buttons, sliders or other elements in the menu.
	f32 nextElementYOffset;				  // Y offset that the next element needs to have to not overlap with everything else in the menu.
} DebugMenu;

// Data about the state of the Debug ui system, only one instance of this struct should exist.
typedef struct DebugUIState
{
    DebugMenu** debugMenuDarray;                  // Dynamic array with all the debug menu instances that exist
    Material menuBackgroundMaterial;              // Material to render quads with.
    QuadInstanceData* menuBackgroundQuads;        // Quad instances for all the menu backgrounds.
    VertexBuffer menuBackgroundsVB;               // Vertex Buffer with the instances for menu background quads
    MeshData* quadMesh;                           // Mesh with the data to make quad instances.
    mat4 uiProjView;                              // Projection and view matrix for all debug menu's
    mat4 inverseProjView;                         // Inverted proj view matrix.
    Allocator* interactableInternalDataAllocator; // Allocator for allocating interactable internal data.
} DebugUIState;

static DebugUIState* state = nullptr;

static void DebugUIAddMenuHandlebar(DebugMenu* menu, const char* text);

// Function prototypes for interactable handling functions, full functions are at the bottom of this file.
void HandleButtonInteractionStart(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition);
void HandleButtonInteractionUpdate(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition);
void HandleButtonInteractionEnd(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition);

void HandleMenuHandlebarInteractionStart(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition);
void HandleMenuHandlebarInteractionUpdate(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition);
void HandleMenuHandlebarInteractionEnd(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition);

void HandleSliderInteractionStart(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition);
void HandleSliderInteractionUpdate(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition);
void HandleSliderInteractionEnd(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition);

static bool OnWindowResize(EventCode type, EventData data)
{
    vec2i windowSize = GetPlatformWindowSize();
    float windowAspectRatio = windowSize.x / (float)windowSize.y;
    state->uiProjView = mat4_orthographic(0, MENU_ORTHO_PROJECTION_HEIGHT * windowAspectRatio, 0, MENU_ORTHO_PROJECTION_HEIGHT, -1, 1);
    state->inverseProjView = mat4_inverse(state->uiProjView);

    return false;
}

static bool PointInRect(vec2 position, vec2 size, vec2 point)
{
    bool left = point.x > position.x;
    bool right = point.x < (position.x + size.x);
    bool top = point.y < (position.y + size.y);
    bool bottom = point.y > position.y;

    return (left && right && top && bottom);
}

bool InitializeDebugUI()
{
    GRASSERT_DEBUG(state == nullptr); // If this triggers, the debug ui was initialized twice.

    state = Alloc(GetGlobalAllocator(), sizeof(*state), MEM_TAG_RENDERER_SUBSYS);
    MemoryZero(state, sizeof(*state));

    // Creating interactable internal data allocator
    CreateFreelistAllocator("DebugUI interactable internal data", GetGlobalAllocator(), ITERACTABLE_INTERNAL_DATA_ALLOCATOR_SIZE, &state->interactableInternalDataAllocator);

    // Creating the quad shader and material
    ShaderCreateInfo shaderCreateInfo = {};
    shaderCreateInfo.renderTargetStencil = false;
    shaderCreateInfo.renderTargetDepth = true;
    shaderCreateInfo.renderTargetColor = true;
    shaderCreateInfo.vertexShaderName = "roundedQuad";
    shaderCreateInfo.fragmentShaderName = "roundedQuad";
    shaderCreateInfo.vertexBufferLayout.perVertexAttributeCount = 3;
    shaderCreateInfo.vertexBufferLayout.perVertexAttributes[0] = VERTEX_ATTRIBUTE_TYPE_VEC3;	// Position
    shaderCreateInfo.vertexBufferLayout.perVertexAttributes[1] = VERTEX_ATTRIBUTE_TYPE_VEC3;	// Normal
    shaderCreateInfo.vertexBufferLayout.perVertexAttributes[2] = VERTEX_ATTRIBUTE_TYPE_VEC2;	// TexCoord
    shaderCreateInfo.vertexBufferLayout.perInstanceAttributeCount = 2;
    shaderCreateInfo.vertexBufferLayout.perInstanceAttributes[0] = VERTEX_ATTRIBUTE_TYPE_MAT4;	// Model Matrix
    shaderCreateInfo.vertexBufferLayout.perInstanceAttributes[1] = VERTEX_ATTRIBUTE_TYPE_VEC4;	// Color

    ShaderCreate("roundedQuad", &shaderCreateInfo);
    state->menuBackgroundMaterial = MaterialCreate(ShaderGetRef("roundedQuad"));

    state->debugMenuDarray = DarrayCreate(sizeof(*state->debugMenuDarray), MAX_DBG_MENUS, GetGlobalAllocator(), MEM_TAG_RENDERER_SUBSYS);

    state->quadMesh = GetBasicMesh(BASIC_MESH_NAME_QUAD);

    vec2i windowSize = GetPlatformWindowSize();
    float windowAspectRatio = windowSize.x / (float)windowSize.y;
    mat4 projection = mat4_orthographic(0, MENU_ORTHO_PROJECTION_HEIGHT * windowAspectRatio, 0, MENU_ORTHO_PROJECTION_HEIGHT, -1, 1);
    mat4 view = mat4_3Dtranslate(vec3_create(0, 0, 0));
    state->uiProjView = mat4_mul_mat4(projection, view);
    state->inverseProjView = mat4_inverse(state->uiProjView);

    state->menuBackgroundQuads = Alloc(GetGlobalAllocator(), sizeof(*state->menuBackgroundQuads) * MAX_DBG_MENUS, MEM_TAG_RENDERER_SUBSYS);
    state->menuBackgroundsVB = VertexBufferCreate(state->menuBackgroundQuads, sizeof(*state->menuBackgroundQuads) * MAX_DBG_MENUS);

    RegisterEventListener(EVCODE_WINDOW_RESIZED, OnWindowResize);

    return true;
}

void ShutdownDebugUI()
{
    UnregisterEventListener(EVCODE_WINDOW_RESIZED, OnWindowResize);

    DestroyFreelistAllocator(state->interactableInternalDataAllocator);
    MaterialDestroy(state->menuBackgroundMaterial);
    DarrayDestroy(state->debugMenuDarray);
    Free(GetGlobalAllocator(), state);
}

void UpdateDebugUI()
{
    // Getting the mouse position in world space.
    vec4 mouseScreenPos = vec4_create(GetMousePos().x, GetMousePos().y, 0, 1); // A mouse position is 2d but we pad the z with 0 and w with 1, so that we can do matrix math
    vec4 clipCoords = ScreenToClipSpace(mouseScreenPos);
    vec4 mouseWorldPos = mat4_mul_vec4(state->inverseProjView, clipCoords);

    u32 menuCount = DarrayGetSize(state->debugMenuDarray);

    // Looping through all the menu's to handle user interaction for each one.
    for (int i = 0; i < menuCount; i++)
    {
        DebugMenu* menu = state->debugMenuDarray[i];

        // If a button or slider is being interacted with already
        if (menu->activeInteractableIndex != NO_INTERACTABLE_ACTIVE_VALUE)
        {
            // If the user let go of their mouse button then interaction end will be called for the active interactable.
            if (!GetButtonDown(BUTTON_LEFTMOUSEBTN))
            {
                void (*interaction_end_func_ptr_arr[])(DebugMenu*, InteractableData*, vec4) = {HandleButtonInteractionEnd, HandleMenuHandlebarInteractionEnd, HandleSliderInteractionEnd};

                GRASSERT_DEBUG(menu->interactablesArray[menu->activeInteractableIndex].interactableType < INTERACTABLE_TYPE_COUNT);

                // Calling the correct InteractionEnd function
                (*interaction_end_func_ptr_arr[menu->interactablesArray[menu->activeInteractableIndex].interactableType])(menu, &menu->interactablesArray[menu->activeInteractableIndex], mouseWorldPos);

                menu->activeInteractableIndex = NO_INTERACTABLE_ACTIVE_VALUE; // Indicating that nothing is being interacted with anymore
            }
            else // If the user is still holding the mouse down, give the interaction update call to the active interactable.
            {
                void (*interaction_update_func_ptr_arr[])(DebugMenu*, InteractableData*, vec4) = {HandleButtonInteractionUpdate, HandleMenuHandlebarInteractionUpdate, HandleSliderInteractionUpdate};

                GRASSERT_DEBUG(menu->interactablesArray[menu->activeInteractableIndex].interactableType < INTERACTABLE_TYPE_COUNT);

                // Calling the correct InteractionUpdate function
                (*interaction_update_func_ptr_arr[menu->interactablesArray[menu->activeInteractableIndex].interactableType])(menu, &menu->interactablesArray[menu->activeInteractableIndex], mouseWorldPos);
            }

			// Breaking out of the menu loop so that only one menu can be interacted with at a time
			break;
        }
        // If NO interactable in this menu is being interacted with and the mouse button was pressed.
        else if (GetButtonDown(BUTTON_LEFTMOUSEBTN) && !GetButtonDownPrevious(BUTTON_LEFTMOUSEBTN))
        {
            // Checking if the mouse is even in this menu
            bool mouseInMenu = PointInRect(menu->position, menu->size, vec4_xy(mouseWorldPos));
            _DEBUG("Menu clicked: %s", mouseInMenu ? "true" : "false");

            // If the mouse is in this menu, loop through all the elements in this menu to see which one needs to be interacted with.
            if (mouseInMenu)
            {
				// Gets set to true if an element is found to be interacted with so we can break out of the menu loop
				bool elementInteractedWith = false;

                for (int j = 0; j < menu->interactablesCount; j++)
                {
                    // If the mouse is on element j, handle the interaction start and set it as the active interactable.
                    if (PointInRect(vec2_add_vec2(menu->interactablesArray[j].position, menu->position), menu->interactablesArray[j].size, vec4_xy(mouseWorldPos)))
                    {
                        // Array of all interaction start functions
                        void (*interaction_start_func_ptr_arr[])(DebugMenu*, InteractableData*, vec4) = {HandleButtonInteractionStart, HandleMenuHandlebarInteractionStart, HandleSliderInteractionStart};

                        GRASSERT_DEBUG(menu->interactablesArray[j].interactableType < INTERACTABLE_TYPE_COUNT);

                        // Calling the correct InteractionStart function
                        (*interaction_start_func_ptr_arr[menu->interactablesArray[j].interactableType])(menu, &menu->interactablesArray[j], mouseWorldPos);

                        menu->activeInteractableIndex = j; // Here j is the index of the interactable that is now being interacted with.
						elementInteractedWith = true;
                        break;
                    }
                }

				// Breaking out of the menu loop if an element was clicked so that only one menu can be interacted with at a time
				if (elementInteractedWith)
					break;
            }
        }
    }
}

DebugMenu* DebugUICreateMenu()
{
	// Allocating the DebugMenu struct
    DebugMenu* menu = Alloc(GetGlobalAllocator(), sizeof(*menu), MEM_TAG_RENDERER_SUBSYS);

	// Positioning the menu
    menu->position = MENU_START_POSITION;
    menu->size = MENU_START_SIZE;
    menu->menuTransform = mat4_mul_mat4(mat4_2Dtranslate(menu->position), mat4_2Dscale(menu->size));
	menu->nextElementYOffset = MENU_ELEMENTS_OFFSET;	// Making sure that the first element that gets added to the menu has the correct offset from the edge of the menu.

	// Creating a quad for the menu in the menuBackgrounds instanced vertex buffer (the base menu is basically just a quad with a color)
	u32 menuIndex = DarrayGetSize(state->debugMenuDarray);
    state->menuBackgroundQuads[menuIndex].transform = menu->menuTransform;
    state->menuBackgroundQuads[menuIndex].color = MENU_BACKGROUND_COLOR;

    VertexBufferUpdate(state->menuBackgroundsVB, state->menuBackgroundQuads, sizeof(*state->menuBackgroundQuads) * MAX_DBG_MENUS);

	// Creating an instanced vertex buffer for all the quads that will be rendered in the menu.
    menu->quadsInstanceData = Alloc(GetGlobalAllocator(), sizeof(*menu->quadsInstanceData) * MAX_DBG_MENU_QUADS, MEM_TAG_RENDERER_SUBSYS);
    menu->maxQuads = MAX_DBG_MENU_QUADS;
    menu->quadCount = 0;
    menu->menuElementMaterial = MaterialCreate(ShaderGetRef("roundedQuad"));

    menu->quadsInstancedVB = VertexBufferCreate(menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * MAX_DBG_MENU_QUADS);

	// Putting the menu struct in the menu d array
    state->debugMenuDarray = DarrayPushback(state->debugMenuDarray, &menu);
    GRASSERT_DEBUG(DarrayGetSize(state->debugMenuDarray) <= MAX_DBG_MENUS);

	// Creating an array for keeping track of all the interactable elements in the menu
    menu->interactablesArray = Alloc(GetGlobalAllocator(), sizeof(*menu->interactablesArray) * MAX_DBG_MENU_INTERACTABLES, MEM_TAG_RENDERER_SUBSYS);
	MemoryZero(menu->interactablesArray, sizeof(*menu->interactablesArray) * MAX_DBG_MENU_INTERACTABLES);
    menu->interactablesCount = 0;
    menu->activeInteractableIndex = NO_INTERACTABLE_ACTIVE_VALUE;

	// Adding the menu handlebar
	DebugUIAddMenuHandlebar(menu, "placeholder text (TODO: make user adjustable)");

    return menu;
}

void DebugUIDestroyMenu(DebugMenu* menu)
{
	// Destroying graphics resources
	VertexBufferDestroy(menu->quadsInstancedVB);
	MaterialDestroy(menu->menuElementMaterial);

	// Freeing simple allocations
	Free(GetGlobalAllocator(), menu->quadsInstanceData);

	// Freeing interactables
	for (int i = 0; i < MAX_DBG_MENU_INTERACTABLES; i++)
	{
		if (menu->interactablesArray[i].internalData)
			Free(state->interactableInternalDataAllocator, menu->interactablesArray[i].internalData);
	}
	Free(GetGlobalAllocator(), menu->interactablesArray);

	// remove the menu from the debugMenuArray and remove the menu quad from the quads darray
	for (int i = 0; i < MAX_DBG_MENUS; i++)
	{
		if (state->debugMenuDarray[i] == menu)
		{
			// Removing the menu quad from the menu background quads
			u32 menuQuadsToCopy = MAX_DBG_MENUS - i;
			if (menuQuadsToCopy > 0)
				MemoryCopy(&state->menuBackgroundQuads[i], &state->menuBackgroundQuads[i + 1], menuQuadsToCopy * sizeof(*state->menuBackgroundQuads));

			// Removing the menu from the debug menu darray
			DarrayPopAt(state->debugMenuDarray, i);
			break;
		}
	}

	// Freeing the menu itself
	Free(GetGlobalAllocator(), menu);
}

void DebugUIRenderMenu(DebugMenu* menu)
{
    MaterialUpdateProperty(state->menuBackgroundMaterial, "menuView", &state->uiProjView);
    MaterialBind(state->menuBackgroundMaterial);

    VertexBuffer menuVertexBuffers[2] = {state->quadMesh->vertexBuffer, state->menuBackgroundsVB};

    Draw(2, menuVertexBuffers, state->quadMesh->indexBuffer, nullptr, DarrayGetSize(state->debugMenuDarray));

	// Z for the view translation is 0.5f so that menu elements are rendered above the menu itself
    mat4 menuElementsView = mat4_mul_mat4(state->uiProjView, mat4_3Dtranslate(vec3_create(menu->position.x, menu->position.y, 0.5f)));
    MaterialUpdateProperty(menu->menuElementMaterial, "menuView", &menuElementsView);
    MaterialBind(menu->menuElementMaterial);

    VertexBuffer vertexBuffers[2] = {state->quadMesh->vertexBuffer, menu->quadsInstancedVB};

    Draw(2, vertexBuffers, state->quadMesh->indexBuffer, nullptr, menu->quadCount);
}

static void DebugUIAddMenuHandlebar(DebugMenu* menu, const char* text)
{
	vec2 handlebarPosition = vec2_create(0, menu->size.y - HANDLEBAR_VERTICAL_SIZE);
	vec2 handlebarSize = vec2_create(menu->size.x, HANDLEBAR_VERTICAL_SIZE);
	mat4 handlebarTransform = mat4_mul_mat4(mat4_2Dtranslate(handlebarPosition), mat4_2Dscale(handlebarSize));
	menu->quadsInstanceData[menu->quadCount].transform = handlebarTransform;
    menu->quadsInstanceData[menu->quadCount].color = HANDLEBAR_COLOR;
    menu->quadCount++;
	
	menu->nextElementYOffset += HANDLEBAR_VERTICAL_SIZE;

	GRASSERT_DEBUG(menu->quadCount <= MAX_DBG_MENU_QUADS);

	VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);

	MenuHandlebarInteractableData* handlebarData = Alloc(state->interactableInternalDataAllocator, sizeof(*handlebarData), MEM_TAG_RENDERER_SUBSYS);
	handlebarData->mouseStartWorldPosition = vec4_create(0, 0, 0, 0);

	menu->interactablesArray[menu->interactablesCount].firstQuad = menu->quadCount - 1; // Minus one because this function added one quad that belongs to the handlebar. 
    menu->interactablesArray[menu->interactablesCount].quadCount = 1;
    menu->interactablesArray[menu->interactablesCount].position = handlebarPosition;
    menu->interactablesArray[menu->interactablesCount].size = handlebarSize;
    menu->interactablesArray[menu->interactablesCount].interactableType = INTERACTABLE_TYPE_MENU_HANDLEBAR;
    menu->interactablesArray[menu->interactablesCount].internalData = handlebarData;

    menu->interactablesCount++;
    GRASSERT_DEBUG(menu->interactablesCount <= MAX_DBG_MENU_INTERACTABLES);
}

void DebugUIAddButton(DebugMenu* menu, const char* text, bool* pStateBool, bool* pSignalBool)
{
	// Allocating internal button data and saving the pointers to the state and signal bools
    ButtonInteractableData* buttonData = Alloc(state->interactableInternalDataAllocator, sizeof(*buttonData), MEM_TAG_RENDERER_SUBSYS);
    buttonData->pStateBool = pStateBool;
    buttonData->pSignalBool = pSignalBool;

	menu->nextElementYOffset += BUTTON_SIZE.y;
    vec2 buttonPosition = vec2_create(MENU_ELEMENTS_OFFSET, menu->size.y - menu->nextElementYOffset);
    vec2 buttonSize = BUTTON_SIZE;
    mat4 buttonTransform = mat4_mul_mat4(mat4_2Dtranslate(buttonPosition), mat4_2Dscale(buttonSize));
    menu->quadsInstanceData[menu->quadCount].transform = buttonTransform;
    menu->quadsInstanceData[menu->quadCount].color = BUTTON_BASIC_COLOR;
    menu->quadCount++;
	menu->nextElementYOffset += MENU_ELEMENTS_OFFSET;

    GRASSERT_DEBUG(menu->quadCount <= MAX_DBG_MENU_QUADS);

    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);

    menu->interactablesArray[menu->interactablesCount].firstQuad = menu->quadCount - 1; // Minus one because this function added one quad that belongs to the button.
    menu->interactablesArray[menu->interactablesCount].quadCount = 1;
    menu->interactablesArray[menu->interactablesCount].position = buttonPosition;
    menu->interactablesArray[menu->interactablesCount].size = buttonSize;
    menu->interactablesArray[menu->interactablesCount].interactableType = INTERACTABLE_TYPE_BUTTON;
    menu->interactablesArray[menu->interactablesCount].internalData = buttonData;

    menu->interactablesCount++;
    GRASSERT_DEBUG(menu->interactablesCount <= MAX_DBG_MENU_INTERACTABLES);
}

void DebugUIAddSlider(DebugMenu* menu, const char* text, f32 minValue, f32 maxValue, f32* pSliderValue)
{
	// Allocating internal slider data and saving the pointer to the slider value
    SliderInteractableData* sliderData = Alloc(state->interactableInternalDataAllocator, sizeof(*sliderData), MEM_TAG_RENDERER_SUBSYS);
    sliderData->pSliderValue = pSliderValue;
	sliderData->minValue = minValue;
	sliderData->maxValue = maxValue;
	sliderData->valueRange = maxValue - minValue;

	// Setting the float at pSliderValue to minValue if it falls outside of min and max value, this value will be the start value of the slider and gets used to position the slider correctly.
	if (*pSliderValue < minValue || *pSliderValue > maxValue)
		*pSliderValue = minValue;

	// Percentage of how much of the slider is filled out.
	f32 sliderProgress = (*pSliderValue - minValue) / sliderData->valueRange;

	// Adding a quad for the slider bar and a quad for the slider dot.
	menu->nextElementYOffset += SLIDER_DOT_SIZE.y;
	vec2 sliderTotalPosition = vec2_create(MENU_ELEMENTS_OFFSET - SLIDER_DOT_SIZE.x / 2, menu->size.y - menu->nextElementYOffset);
	vec2 sliderTotalSize = vec2_create(SLIDER_BAR_SIZE.x + SLIDER_DOT_SIZE.x, SLIDER_DOT_SIZE.y);
	vec2 sliderBarPosition = vec2_create(MENU_ELEMENTS_OFFSET, menu->size.y - menu->nextElementYOffset + (SLIDER_DOT_SIZE.y / 4));// Y position is slightly different than that of the dot because the origin of the quad is the bottom left corner.
    vec2 sliderBarSize = SLIDER_BAR_SIZE;
    mat4 sliderBarTransform = mat4_mul_mat4(mat4_2Dtranslate(sliderBarPosition), mat4_2Dscale(sliderBarSize));
    menu->quadsInstanceData[menu->quadCount].transform = sliderBarTransform;
    menu->quadsInstanceData[menu->quadCount].color = SLIDER_BAR_COLOR;
    menu->quadCount++;
	vec3 sliderDotPosition = vec3_create((MENU_ELEMENTS_OFFSET - SLIDER_DOT_SIZE.x / 2) + sliderProgress * sliderBarSize.x, menu->size.y - menu->nextElementYOffset, 0.1f); // Slider dot gets a z position to position it in front of the slider bar.
	vec2 sliderDotSize = SLIDER_DOT_SIZE;
	mat4 sliderDotTransform = mat4_mul_mat4(mat4_3Dtranslate(sliderDotPosition), mat4_2Dscale(sliderDotSize));
	menu->quadsInstanceData[menu->quadCount].transform = sliderDotTransform;
    menu->quadsInstanceData[menu->quadCount].color = SLIDER_DOT_COLOR;
    menu->quadCount++;
	menu->nextElementYOffset += MENU_ELEMENTS_OFFSET;

	sliderData->sliderDotHeight	= sliderDotPosition.y;

    GRASSERT_DEBUG(menu->quadCount <= MAX_DBG_MENU_QUADS);

    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);

    menu->interactablesArray[menu->interactablesCount].firstQuad = menu->quadCount - 2; // Minus two because this function added two quads that belong to the slider.
    menu->interactablesArray[menu->interactablesCount].quadCount = 2;
    menu->interactablesArray[menu->interactablesCount].position = sliderTotalPosition;
    menu->interactablesArray[menu->interactablesCount].size = sliderTotalSize;
    menu->interactablesArray[menu->interactablesCount].interactableType = INTERACTABLE_TYPE_SLIDER;
    menu->interactablesArray[menu->interactablesCount].internalData = sliderData;

    menu->interactablesCount++;
    GRASSERT_DEBUG(menu->interactablesCount <= MAX_DBG_MENU_INTERACTABLES);
}

// =========================================================================================================================================
// Interaction start/update/end functions
// =========================================================================================================================================

/// ============================================ Button
void HandleButtonInteractionStart(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition)
{
    // Set the button to the pressed color
    menu->quadsInstanceData[interactableData->firstQuad].color = BUTTON_PRESSED_COLOR;
    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);

    // Set the state of the button to being pressed
    ButtonInteractableData* buttonData = interactableData->internalData;
	if (buttonData->pStateBool)
    	*buttonData->pStateBool = true;
}

void HandleButtonInteractionUpdate(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition)
{
}

void HandleButtonInteractionEnd(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition)
{
    // Changing the color of the button back to the non-pressed color
    menu->quadsInstanceData[interactableData->firstQuad].color = BUTTON_BASIC_COLOR;
    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);

	// Set the state of the button to not being pressed
    ButtonInteractableData* buttonData = interactableData->internalData;
	if (buttonData->pStateBool)
    	*buttonData->pStateBool = false;

	// If the player was still hovering over the button when they released it send a signal that the button was pressed.
    if (buttonData->pSignalBool && PointInRect(vec2_add_vec2(interactableData->position, menu->position), interactableData->size, vec4_xy(mouseWorldPosition)))
    {
		*buttonData->pSignalBool = true;
    }
}

/// ============================================= Menu Handlebar
void HandleMenuHandlebarInteractionStart(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition)
{
	// Set the handlebar to the pressed color
    menu->quadsInstanceData[interactableData->firstQuad].color = HANDLEBAR_PRESSED_COLOR;
    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);

    // Set the position of the mouse when the user first clicked on the handlebar
    MenuHandlebarInteractableData* handlebarData = interactableData->internalData;
	handlebarData->mouseStartWorldPosition = mouseWorldPosition;
	handlebarData->menuStartPosition = menu->position;
}

void HandleMenuHandlebarInteractionUpdate(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition)
{
	// Moving the menu based on how the mouse has moved
	MenuHandlebarInteractableData* handlebarData = interactableData->internalData;
	vec4 mouseDeltaWorldPosition = vec4_sub_vec4(mouseWorldPosition, handlebarData->mouseStartWorldPosition);
	menu->position = vec2_add_vec2(vec4_xy(mouseDeltaWorldPosition), handlebarData->menuStartPosition);

	// Updating the menu backround vertex buffer
	menu->menuTransform = mat4_mul_mat4(mat4_2Dtranslate(menu->position), mat4_2Dscale(menu->size));
	u32 menuIndex = 0;
	for (u32 i = 0; i < MAX_DBG_MENUS; i++)
	{
		if (state->debugMenuDarray[i] == menu)
		{
			menuIndex = i;
			break;
		}
	}
    state->menuBackgroundQuads[menuIndex].transform = menu->menuTransform;
    VertexBufferUpdate(state->menuBackgroundsVB, state->menuBackgroundQuads, sizeof(*state->menuBackgroundQuads) * MAX_DBG_MENUS);
}

void HandleMenuHandlebarInteractionEnd(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition)
{
	// Changing the color of the handlebar back to the non-pressed color
    menu->quadsInstanceData[interactableData->firstQuad].color = HANDLEBAR_COLOR;
    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);

	// Moving the menu based on how the mouse has moved
	MenuHandlebarInteractableData* handlebarData = interactableData->internalData;
	vec4 mouseDeltaWorldPosition = vec4_sub_vec4(mouseWorldPosition, handlebarData->mouseStartWorldPosition);
	menu->position = vec2_add_vec2(vec4_xy(mouseDeltaWorldPosition), handlebarData->menuStartPosition);

	// Updating the menu backround vertex buffer
	menu->menuTransform = mat4_mul_mat4(mat4_2Dtranslate(menu->position), mat4_2Dscale(menu->size));
    u32 menuIndex = 0;
	for (u32 i = 0; i < MAX_DBG_MENUS; i++)
	{
		if (state->debugMenuDarray[i] == menu)
		{
			menuIndex = i;
			break;
		}
	}
    state->menuBackgroundQuads[menuIndex].transform = menu->menuTransform;
    VertexBufferUpdate(state->menuBackgroundsVB, state->menuBackgroundQuads, sizeof(*state->menuBackgroundQuads) * MAX_DBG_MENUS);
}


/// ============================================= Slider
void HandleSliderInteractionStart(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition)
{
	SliderInteractableData* sliderData = interactableData->internalData;

	// Calculating the percentage of slider completion based on the mouse position. It doesn't have to be mapped from 0-1 because it is already in the world position range.
	f32 mouseSliderPosition = mouseWorldPosition.x - (menu->position.x + MENU_ELEMENTS_OFFSET/*this represents the start of the slider*/);
	if (mouseSliderPosition > SLIDER_BAR_SIZE.x/*this represents the length of the slider*/)
		mouseSliderPosition = SLIDER_BAR_SIZE.x;
	if (mouseSliderPosition < 0)
		mouseSliderPosition = 0;

	vec3 sliderDotPosition = vec3_create((MENU_ELEMENTS_OFFSET - (SLIDER_DOT_SIZE.x / 2)) + mouseSliderPosition, sliderData->sliderDotHeight, 0.1f); // Slider dot gets a z position to position it in front of the slider bar.
	vec2 sliderDotSize = SLIDER_DOT_SIZE;
	mat4 sliderDotTransform = mat4_mul_mat4(mat4_3Dtranslate(sliderDotPosition), mat4_2Dscale(sliderDotSize));
	menu->quadsInstanceData[interactableData->firstQuad + 1/*indexing into the dot quad*/].transform = sliderDotTransform;
    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);

	// Updating the slider value for the client.
	mouseSliderPosition /= SLIDER_BAR_SIZE.x; // Normalizing mouseSliderPosition (or remapping it to [0, 1])
	*sliderData->pSliderValue = sliderData->minValue + (mouseSliderPosition * sliderData->valueRange);
}

void HandleSliderInteractionUpdate(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition)
{
	SliderInteractableData* sliderData = interactableData->internalData;

	// Calculating the percentage of slider completion based on the mouse position. It doesn't have to be mapped from 0-1 because it is already in the world position range.
	f32 mouseSliderPosition = mouseWorldPosition.x - (menu->position.x + MENU_ELEMENTS_OFFSET/*this represents the start of the slider*/);
	if (mouseSliderPosition > SLIDER_BAR_SIZE.x/*this represents the length of the slider*/)
		mouseSliderPosition = SLIDER_BAR_SIZE.x;
	if (mouseSliderPosition < 0)
		mouseSliderPosition = 0;

	vec3 sliderDotPosition = vec3_create((MENU_ELEMENTS_OFFSET - (SLIDER_DOT_SIZE.x / 2)) + mouseSliderPosition, sliderData->sliderDotHeight, 0.1f); // Slider dot gets a z position to position it in front of the slider bar.
	vec2 sliderDotSize = SLIDER_DOT_SIZE;
	mat4 sliderDotTransform = mat4_mul_mat4(mat4_3Dtranslate(sliderDotPosition), mat4_2Dscale(sliderDotSize));
	menu->quadsInstanceData[interactableData->firstQuad + 1/*indexing into the dot quad*/].transform = sliderDotTransform;
    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);

	// Updating the slider value for the client.
	mouseSliderPosition /= SLIDER_BAR_SIZE.x; // Normalizing mouseSliderPosition (or remapping it to [0, 1])
	*sliderData->pSliderValue = sliderData->minValue + (mouseSliderPosition * sliderData->valueRange);
}

void HandleSliderInteractionEnd(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition)
{
	SliderInteractableData* sliderData = interactableData->internalData;

	// Calculating the percentage of slider completion based on the mouse position. It doesn't have to be mapped from 0-1 because it is already in the world position range.
	f32 mouseSliderPosition = mouseWorldPosition.x - (menu->position.x + MENU_ELEMENTS_OFFSET/*this represents the start of the slider*/);
	if (mouseSliderPosition > SLIDER_BAR_SIZE.x/*this represents the length of the slider*/)
		mouseSliderPosition = SLIDER_BAR_SIZE.x;
	if (mouseSliderPosition < 0)
		mouseSliderPosition = 0;

	vec3 sliderDotPosition = vec3_create((MENU_ELEMENTS_OFFSET - (SLIDER_DOT_SIZE.x / 2)) + mouseSliderPosition, sliderData->sliderDotHeight, 0.1f); // Slider dot gets a z position to position it in front of the slider bar.
	vec2 sliderDotSize = SLIDER_DOT_SIZE;
	mat4 sliderDotTransform = mat4_mul_mat4(mat4_3Dtranslate(sliderDotPosition), mat4_2Dscale(sliderDotSize));
	menu->quadsInstanceData[interactableData->firstQuad + 1/*indexing into the dot quad*/].transform = sliderDotTransform;
    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);

	// Updating the slider value for the client.
	mouseSliderPosition /= SLIDER_BAR_SIZE.x; // Normalizing mouseSliderPosition (or remapping it to [0, 1])
	*sliderData->pSliderValue = sliderData->minValue + (mouseSliderPosition * sliderData->valueRange);
}
