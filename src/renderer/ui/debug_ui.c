#include "debug_ui.h"

#include "core/asserts.h"
#include "core/event.h"
#include "core/input.h"
#include "core/meminc.h"
#include "core/platform.h"
#include "math/lin_alg.h"
#include "renderer/renderer.h"
#include "renderer/ui/text_renderer.h"

// STEPS TO ADD A NEW INTERACTABLE TYPE ==================================================================
// Add it to the InteractableType enum
// Increase the INTERACTABLE_TYPE_COUNT define by one
// Add a struct named TypeInteractableData with the data that is important to that specific interactable type
// Add a DebugUIAdd*Type* function declaration to the .h file
// Create the definition for said function
// Add the function prototypes for start, update and end: Handle*Type*Interaction#Start/Update/End#
// Add the function definitions for these functions
// Add these functions to the lists of function pointers in the UpdateDebugUI function (in the correct position, same position as it is in the enum)
// Done!

#define MAX_DBG_MENU_QUADS 100                             // Change if you need more
#define MAX_DBG_MENU_INTERACTABLES 20                      // Change if you need more
#define MAX_DBG_MENUS 3                                    // Change if you need more
#define ITERACTABLE_INTERNAL_DATA_ALLOCATOR_SIZE (KiB * 5) // This size is arbitrary :)
#define NO_INTERACTABLE_ACTIVE_VALUE -1

// ====================== Debug menu visual constant values =====================================
#define MENU_ORTHO_PROJECTION_HEIGHT 10
#define MENU_START_POSITION vec2_create(0.3f, 0.3f)
#define MENU_START_SIZE vec2_create(2.5f, 9.4f)
#define MENU_BACKGROUND_COLOR vec4_create(44.f/255.f, 62.f/255.f, 80.f/255.f, 1)
#define HANDLEBAR_VERTICAL_SIZE 0.5f
#define HANDLEBAR_COLOR vec4_create(52.f/255.f, 73.f/255.f, 94.f/255.f, 1)
#define HANDLEBAR_PRESSED_COLOR vec4_create(44.f/255.f, 62.f/255.f, 80.f/255.f, 1)
#define MENU_ELEMENTS_OFFSET 0.3f
#define BUTTON_SIZE vec2_create(1, 1)
#define BUTTON_BASIC_COLOR vec4_create(155.f/255.f, 89.f/255.f, 182.f/255.f, 1)
#define BUTTON_PRESSED_COLOR vec4_create(142.f/255.f, 68.f/255.f, 173.f/255.f, 1.0f)
#define SLIDER_BAR_SIZE vec2_create(2, 0.2f)
#define SLIDER_DOT_SIZE vec2_create(0.4f, 0.4f)
#define SLIDER_BAR_COLOR vec4_create(52.f/255.f, 152.f/255.f, 219.f/255.f, 1)
#define SLIDER_DOT_COLOR vec4_create(155.f/255.f, 89.f/255.f, 182.f/255.f, 1)
#define GREY_OUT_FACTOR 0.8f
#define MENU_ELEMENT_OUTLINE_COLOR vec4_create(26.f/255.f, 188.f/255.f, 156.f/255.f, 1)
#define MENU_ELEMENT_OUTLINE_DATA vec4_create(0.03f, 0.03f, 0, 0)
#define ELEMENT_TITLE_TEXT_SIZE .05f
#define MENU_TITLE_TEXT_SIZE .1f


typedef struct QuadInstanceData
{
    mat4 transform;
    vec4 color;
} QuadInstanceData;

typedef enum InteractableType
{
    INTERACTABLE_TYPE_BUTTON,
    INTERACTABLE_TYPE_TOGGLE_BUTTON,
    INTERACTABLE_TYPE_MENU_HANDLEBAR,
    INTERACTABLE_TYPE_SLIDER_FLOAT,
    INTERACTABLE_TYPE_SLIDER_INT,
    INTERACTABLE_TYPE_SLIDER_DISCRETE,
    INTERACTABLE_TYPE_SLIDER_LOG,
    INTERACTABLE_TYPE_NONE_COUNT
} InteractableType;

#define INTERACTABLE_TYPE_COUNT 7

// Interactable internal data for a button
typedef struct ButtonInteractableData
{
    bool* pStateBool;  // Pointer to a bool that will store whether the button is currently being held down or not.
    bool* pSignalBool; // Pointer to a bool that will be set to true by the debug ui whenever the button is pressed (pressed = clicked on and let go by the player).
} ButtonInteractableData;

// Interactable internal data for a toggle button
typedef struct ToggleButtonInteractableData
{
    bool* pStateBool; // Pointer to a bool that will store whether the button is currently being held down or not.
} ToggleButtonInteractableData;

// Interactable internal data for the menu handlebar
typedef struct MenuHandlebarInteractableData
{
    vec4 mouseStartWorldPosition; // Position of the mouse when it first clicked on the handlebar to move it.
    vec2 menuStartPosition;       // Position of the menu when the player first clicked on the handlebar to move it.
} MenuHandlebarInteractableData;

// Interactable internal data for the slider float
typedef struct SliderFloatInteractableData
{
    f32* pSliderValue; // Pointer to a float that stores the value of the slider for the client to read out.
    f32 minValue;      // Minimum value of the slider.
    f32 maxValue;      // Maximum value of the slider.
    f32 valueRange;    // Max value - Min value. Used for calculating the position of the slider dot.
} SliderFloatInteractableData;

// Interactable internal data for the slider int
typedef struct SliderIntInteractableData
{
    i64* pSliderValue; // Pointer to an int that stores the value of the slider for the client to read out.
    i64 minValue;      // Minimum value of the slider.
    i64 maxValue;      // Maximum value of the slider.
    u64 valueRange;    // Max value - Min value. Used for calculating the position of the slider dot.
} SliderIntInteractableData;

// Interactable internal data for the slider discrete
typedef struct SliderDiscreteInteractableData
{
    i64* pSliderValue;          // Value that the slider represents, the position of the slider can't be inferred from this, unless you compare the value to the discrete value array
    i64* pDiscreteSliderValues; // Pointer to an float that stores the value of the slider for the client to read out.
    // Min index not necessary because it will always be zero
    u64 maxIndex; // Maximum index of the slider.
                  // value range is also not necessary because it is equal to maxIndex
} SliderDiscreteInteractableData;

// Interactable internal data for the slider log
typedef struct SliderLogInteractableData
{
    // Pointer to a float that stores the exponentiated value of the slider for the client to read out.
    // This is the true value that the slider represents for the user, but not the position of the slider as it isn't linear but exponential.
    f32* pSliderValue;
    f32 base;               // Base used in the log
    f32 minExponentValue;   // Minimum exponent value of the slider.
    f32 maxExponentValue;   // Maximum exponent value of the slider.
    f32 exponentValueRange; // Max exponent value - Min exponent value. Used for calculating the position of the slider dot.
} SliderLogInteractableData;

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
    vec2 position;                        	// Position, anchor is bottom left of the menu.
    vec2 size;                            	// Vec 2 with the menu size, x is width y is height.
    InteractableData* interactablesArray; 	// Array of interactables (buttons, sliders, text, etc.).
    QuadInstanceData* quadsInstanceData;  	// Instance data of the quads to render on the CPU.
    VertexBuffer quadsInstancedVB;        	// Vertex buffer with model matrices for quads.
    Material menuElementMaterial;         	// Material to render quads with.
	TextBatch* elementTextBatch;			// Text batch for rendering the names of elements
    i32 activeInteractableIndex;          	// Index into interactables array, is -1 if no interactable is being interacted with. (If a button is being pressed or a slider is being dragged, this will indicate that.)
    u32 maxQuads;                         	// Max amount of quads
    u32 quadCount;                        	// Current amount of quads
    u32 interactablesCount;               	// Amount of buttons, sliders or other elements in the menu.
    f32 nextElementYOffset;               	// Y offset that the next element needs to have to not overlap with everything else in the menu.
    bool active;
} DebugMenu;

DEFINE_DARRAY_TYPE_REF(DebugMenu);

// Data about the state of the Debug ui system, only one instance of this struct should exist.
typedef struct DebugUIState
{
    DebugMenuRefDarray* debugMenuDarray;          // Dynamic array with all the debug menu instances that exist
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

void HandleToggleButtonInteractionStart(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition);
void HandleToggleButtonInteractionUpdate(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition);
void HandleToggleButtonInteractionEnd(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition);

void HandleMenuHandlebarInteractionStart(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition);
void HandleMenuHandlebarInteractionUpdate(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition);
void HandleMenuHandlebarInteractionEnd(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition);

void HandleSliderFloatInteractionStart(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition);
void HandleSliderFloatInteractionUpdate(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition);
void HandleSliderFloatInteractionEnd(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition);

void HandleSliderIntInteractionStart(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition);
void HandleSliderIntInteractionUpdate(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition);
void HandleSliderIntInteractionEnd(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition);

void HandleSliderDiscreteInteractionStart(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition);
void HandleSliderDiscreteInteractionUpdate(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition);
void HandleSliderDiscreteInteractionEnd(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition);

void HandleSliderLogInteractionStart(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition);
void HandleSliderLogInteractionUpdate(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition);
void HandleSliderLogInteractionEnd(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition);

static bool OnWindowResize(EventCode type, EventData data)
{
    vec2i windowSize = GetPlatformWindowSize();
    f32 windowAspectRatio = windowSize.x / (f32)windowSize.y;
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
    GRASSERT_DEBUG(INTERACTABLE_TYPE_NONE_COUNT == INTERACTABLE_TYPE_COUNT);
	
	TextLoadFont(DEBUG_UI_FONT_NAME, "Roboto-Black.ttf");

    state = Alloc(GetGlobalAllocator(), sizeof(*state));
    MemoryZero(state, sizeof(*state));

    // Creating interactable internal data allocator
    CreateFreelistAllocator("DebugUI interactable internal data", GetGlobalAllocator(), ITERACTABLE_INTERNAL_DATA_ALLOCATOR_SIZE, &state->interactableInternalDataAllocator, true);

    // Creating the quad shader and material
    ShaderCreateInfo shaderCreateInfo = {};
    shaderCreateInfo.renderTargetStencil = false;
    shaderCreateInfo.renderTargetDepth = true;
    shaderCreateInfo.renderTargetColor = true;
    shaderCreateInfo.vertexShaderName = "roundedQuad";
    shaderCreateInfo.fragmentShaderName = "roundedQuad";
    shaderCreateInfo.vertexBufferLayout.perVertexAttributeCount = 3;
    shaderCreateInfo.vertexBufferLayout.perVertexAttributes[0] = VERTEX_ATTRIBUTE_TYPE_VEC3; // Position
    shaderCreateInfo.vertexBufferLayout.perVertexAttributes[1] = VERTEX_ATTRIBUTE_TYPE_VEC3; // Normal
    shaderCreateInfo.vertexBufferLayout.perVertexAttributes[2] = VERTEX_ATTRIBUTE_TYPE_VEC2; // TexCoord
    shaderCreateInfo.vertexBufferLayout.perInstanceAttributeCount = 2;
    shaderCreateInfo.vertexBufferLayout.perInstanceAttributes[0] = VERTEX_ATTRIBUTE_TYPE_MAT4; // Model Matrix
    shaderCreateInfo.vertexBufferLayout.perInstanceAttributes[1] = VERTEX_ATTRIBUTE_TYPE_VEC4; // Color

    ShaderCreate("roundedQuad", &shaderCreateInfo);

    state->debugMenuDarray = DebugMenuRefDarrayCreate(MAX_DBG_MENUS, GetGlobalAllocator());

    state->quadMesh = GetBasicMesh(BASIC_MESH_NAME_QUAD);

    vec2i windowSize = GetPlatformWindowSize();
    f32 windowAspectRatio = windowSize.x / (f32)windowSize.y;
    mat4 projection = mat4_orthographic(0, MENU_ORTHO_PROJECTION_HEIGHT * windowAspectRatio, 0, MENU_ORTHO_PROJECTION_HEIGHT, -1, 1);
    mat4 view = mat4_3Dtranslate(vec3_create(0, 0, 0));
    state->uiProjView = mat4_mul_mat4(projection, view);
    state->inverseProjView = mat4_inverse(state->uiProjView);

    RegisterEventListener(EVCODE_WINDOW_RESIZED, OnWindowResize);

    return true;
}

void ShutdownDebugUI()
{
    UnregisterEventListener(EVCODE_WINDOW_RESIZED, OnWindowResize);

    DestroyFreelistAllocator(state->interactableInternalDataAllocator);
    DarrayDestroy(state->debugMenuDarray);
    Free(GetGlobalAllocator(), state);

	TextUnloadFont(DEBUG_UI_FONT_NAME);
}

void UpdateDebugUI()
{
    void (*interaction_start_func_ptr_arr[])(DebugMenu*, InteractableData*, vec4) =
        {
            HandleButtonInteractionStart,
            HandleToggleButtonInteractionStart,
            HandleMenuHandlebarInteractionStart,
            HandleSliderFloatInteractionStart,
            HandleSliderIntInteractionStart,
            HandleSliderDiscreteInteractionStart,
            HandleSliderLogInteractionStart,
        };
    GRASSERT_DEBUG(INTERACTABLE_TYPE_COUNT == (sizeof(interaction_start_func_ptr_arr) / sizeof(*interaction_start_func_ptr_arr)))

    void (*interaction_update_func_ptr_arr[])(DebugMenu*, InteractableData*, vec4) =
        {
            HandleButtonInteractionUpdate,
            HandleToggleButtonInteractionUpdate,
            HandleMenuHandlebarInteractionUpdate,
            HandleSliderFloatInteractionUpdate,
            HandleSliderIntInteractionUpdate,
            HandleSliderDiscreteInteractionUpdate,
            HandleSliderLogInteractionUpdate,
        };
    GRASSERT_DEBUG(INTERACTABLE_TYPE_COUNT == (sizeof(interaction_update_func_ptr_arr) / sizeof(*interaction_update_func_ptr_arr)))

    void (*interaction_end_func_ptr_arr[INTERACTABLE_TYPE_COUNT])(DebugMenu*, InteractableData*, vec4) =
        {
            HandleButtonInteractionEnd,
            HandleToggleButtonInteractionEnd,
            HandleMenuHandlebarInteractionEnd,
            HandleSliderFloatInteractionEnd,
            HandleSliderIntInteractionEnd,
            HandleSliderDiscreteInteractionEnd,
            HandleSliderLogInteractionEnd,
        };
    GRASSERT_DEBUG(INTERACTABLE_TYPE_COUNT == (sizeof(interaction_end_func_ptr_arr) / sizeof(*interaction_end_func_ptr_arr)))

    // Getting the mouse position in world space.
    vec4 mouseScreenPos = vec4_create(GetMousePos().x, GetMousePos().y, 0, 1); // A mouse position is 2d but we pad the z with 0 and w with 1, so that we can do matrix math
    vec4 clipCoords = ScreenToClipSpace(mouseScreenPos);
    vec4 mouseWorldPos = mat4_mul_vec4(state->inverseProjView, clipCoords);

    u32 menuCount = state->debugMenuDarray->size;

    // Looping through all the menu's to handle user interaction for each one.
    for (int i = 0; i < menuCount; i++)
    {
        DebugMenu* menu = state->debugMenuDarray->data[i];

        if (!menu->active)
            continue;

        // If a button or slider is being interacted with already
        if (menu->activeInteractableIndex != NO_INTERACTABLE_ACTIVE_VALUE)
        {
            // If the user let go of their mouse button then interaction end will be called for the active interactable.
            if (!GetButtonDown(BUTTON_LEFTMOUSEBTN))
            {
                GRASSERT_DEBUG(menu->interactablesArray[menu->activeInteractableIndex].interactableType < INTERACTABLE_TYPE_COUNT);

                // Calling the correct InteractionEnd function
                (*interaction_end_func_ptr_arr[menu->interactablesArray[menu->activeInteractableIndex].interactableType])(menu, &menu->interactablesArray[menu->activeInteractableIndex], mouseWorldPos);

                menu->activeInteractableIndex = NO_INTERACTABLE_ACTIVE_VALUE; // Indicating that nothing is being interacted with anymore
            }
            else // If the user is still holding the mouse down, give the interaction update call to the active interactable.
            {
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
    DebugMenu* menu = Alloc(GetGlobalAllocator(), sizeof(*menu));

    menu->active = true;

    // Positioning the menu
    menu->position = MENU_START_POSITION;
    menu->size = MENU_START_SIZE;
    menu->nextElementYOffset = MENU_ELEMENTS_OFFSET; // Making sure that the first element that gets added to the menu has the correct offset from the edge of the menu.

	// Initiating the menu's text batches
	menu->elementTextBatch = TextBatchCreate(DEBUG_UI_FONT_NAME);

    // Creating an instanced vertex buffer for all the quads that will be rendered in the menu.
    menu->quadsInstanceData = Alloc(GetGlobalAllocator(), sizeof(*menu->quadsInstanceData) * MAX_DBG_MENU_QUADS);
    menu->maxQuads = MAX_DBG_MENU_QUADS;
    menu->quadCount = 0;
    menu->menuElementMaterial = MaterialCreate(ShaderGetRef("roundedQuad"));

    // Creating a quad for the menu background
    menu->quadsInstanceData[menu->quadCount].transform = mat4_mul_mat4(mat4_3Dtranslate(vec3_create(0, 0, -0.5f)), mat4_2Dscale(menu->size));
    menu->quadsInstanceData[menu->quadCount].color = MENU_BACKGROUND_COLOR;
    menu->quadCount++;

    GRASSERT_DEBUG(menu->quadCount <= MAX_DBG_MENU_QUADS);

    menu->quadsInstancedVB = VertexBufferCreate(menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * MAX_DBG_MENU_QUADS);

    // Putting the menu struct in the menu d array
    DebugMenuRefDarrayPushback(state->debugMenuDarray, &menu);
    GRASSERT_DEBUG(state->debugMenuDarray->size <= MAX_DBG_MENUS);

    // Creating an array for keeping track of all the interactable elements in the menu
    menu->interactablesArray = Alloc(GetGlobalAllocator(), sizeof(*menu->interactablesArray) * MAX_DBG_MENU_INTERACTABLES);
    MemoryZero(menu->interactablesArray, sizeof(*menu->interactablesArray) * MAX_DBG_MENU_INTERACTABLES);
    menu->interactablesCount = 0;
    menu->activeInteractableIndex = NO_INTERACTABLE_ACTIVE_VALUE;

    // Adding the menu handlebar
    DebugUIAddMenuHandlebar(menu, "placeholder text (TODO: make user adjustable)");

    return menu;
}

void DebugUIDestroyMenu(DebugMenu* menu)
{
	// Destroying text batches
	TextBatchDestroy(menu->elementTextBatch);

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
        if (state->debugMenuDarray->data[i] == menu)
        {
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
    if (!menu->active)
        return;

    // Z for the view translation is 0.5f so that menu elements are rendered above the menu itself
    mat4 menuElementsView = mat4_mul_mat4(state->uiProjView, mat4_3Dtranslate(vec3_create(menu->position.x, menu->position.y, 0.5f)));
	vec4 outlineColor = MENU_ELEMENT_OUTLINE_COLOR;
	vec4 outlineData = MENU_ELEMENT_OUTLINE_DATA;
    MaterialUpdateProperty(menu->menuElementMaterial, "menuView", &menuElementsView);
	MaterialUpdateProperty(menu->menuElementMaterial, "color", &outlineColor);
    MaterialUpdateProperty(menu->menuElementMaterial, "other", &outlineData);
    MaterialBind(menu->menuElementMaterial);

    VertexBuffer vertexBuffers[2] = {state->quadMesh->vertexBuffer, menu->quadsInstancedVB};

    Draw(2, vertexBuffers, state->quadMesh->indexBuffer, nullptr, menu->quadCount);

	TextBatchRender(menu->elementTextBatch, menuElementsView);
}

void DebugUISetMaterialValues(DebugMenu* menu, vec4 color, vec4 other)
{
	MaterialUpdateProperty(menu->menuElementMaterial, "color", &color);
    MaterialUpdateProperty(menu->menuElementMaterial, "other", &other);
}

void DebugUIMenuSetActive(DebugMenu* menu, bool active)
{
    menu->active = active;
}

static void DebugUIAddMenuHandlebar(DebugMenu* menu, const char* text)
{
    vec2 handlebarPosition = vec2_create(0, menu->size.y - HANDLEBAR_VERTICAL_SIZE);
    vec2 handlebarSize = vec2_create(menu->size.x, HANDLEBAR_VERTICAL_SIZE);
    mat4 handlebarTransform = mat4_mul_mat4(mat4_2Dtranslate(handlebarPosition), mat4_2Dscale(handlebarSize));
    menu->quadsInstanceData[menu->quadCount].transform = handlebarTransform;
    menu->quadsInstanceData[menu->quadCount].color = HANDLEBAR_COLOR;
    menu->quadCount++;

	vec2 menuTitlePosition = vec2_create(MENU_ELEMENTS_OFFSET, handlebarPosition.y + (HANDLEBAR_VERTICAL_SIZE / 2.f) - (MENU_TITLE_TEXT_SIZE / 2.f));

	TextBatchAddText(menu->elementTextBatch, text, menuTitlePosition, MENU_TITLE_TEXT_SIZE, false);

    menu->nextElementYOffset += HANDLEBAR_VERTICAL_SIZE;

    GRASSERT_DEBUG(menu->quadCount <= MAX_DBG_MENU_QUADS);

    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);

    MenuHandlebarInteractableData* handlebarData = Alloc(state->interactableInternalDataAllocator, sizeof(*handlebarData));
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
    ButtonInteractableData* buttonData = Alloc(state->interactableInternalDataAllocator, sizeof(*buttonData));
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

void DebugUIAddToggleButton(DebugMenu* menu, const char* text, bool* pStateBool)
{
    GRASSERT_DEBUG(pStateBool);

    // Allocating internal button data and saving the pointers to the state bool
    ToggleButtonInteractableData* buttonData = Alloc(state->interactableInternalDataAllocator, sizeof(*buttonData));
    buttonData->pStateBool = pStateBool;

    menu->nextElementYOffset += BUTTON_SIZE.y;
    vec2 buttonPosition = vec2_create(MENU_ELEMENTS_OFFSET, menu->size.y - menu->nextElementYOffset);
    vec2 buttonSize = BUTTON_SIZE;
    mat4 buttonTransform = mat4_mul_mat4(mat4_2Dtranslate(buttonPosition), mat4_2Dscale(buttonSize));
    menu->quadsInstanceData[menu->quadCount].transform = buttonTransform;
    if (*pStateBool)
        menu->quadsInstanceData[menu->quadCount].color = BUTTON_PRESSED_COLOR;
    else
        menu->quadsInstanceData[menu->quadCount].color = BUTTON_BASIC_COLOR;
    menu->quadCount++;
    menu->nextElementYOffset += MENU_ELEMENTS_OFFSET;

    GRASSERT_DEBUG(menu->quadCount <= MAX_DBG_MENU_QUADS);

    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);

    menu->interactablesArray[menu->interactablesCount].firstQuad = menu->quadCount - 1; // Minus one because this function added one quad that belongs to the button.
    menu->interactablesArray[menu->interactablesCount].quadCount = 1;
    menu->interactablesArray[menu->interactablesCount].position = buttonPosition;
    menu->interactablesArray[menu->interactablesCount].size = buttonSize;
    menu->interactablesArray[menu->interactablesCount].interactableType = INTERACTABLE_TYPE_TOGGLE_BUTTON;
    menu->interactablesArray[menu->interactablesCount].internalData = buttonData;

    menu->interactablesCount++;
    GRASSERT_DEBUG(menu->interactablesCount <= MAX_DBG_MENU_INTERACTABLES);
}

void DebugUIAddSliderFloat(DebugMenu* menu, const char* text, f32 minValue, f32 maxValue, f32* pSliderValue)
{
    // Allocating internal slider data and saving the pointer to the slider value
    SliderFloatInteractableData* sliderData = Alloc(state->interactableInternalDataAllocator, sizeof(*sliderData));
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
    vec2 sliderBarPosition = vec2_create(MENU_ELEMENTS_OFFSET, menu->size.y - menu->nextElementYOffset + (SLIDER_DOT_SIZE.y / 4)); // Y position is slightly different than that of the dot because the origin of the quad is the bottom left corner.
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

    GRASSERT_DEBUG(menu->quadCount <= MAX_DBG_MENU_QUADS);

    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);

    menu->interactablesArray[menu->interactablesCount].firstQuad = menu->quadCount - 2; // Minus two because this function added two quads that belong to the slider.
    menu->interactablesArray[menu->interactablesCount].quadCount = 2;
    menu->interactablesArray[menu->interactablesCount].position = sliderTotalPosition;
    menu->interactablesArray[menu->interactablesCount].size = sliderTotalSize;
    menu->interactablesArray[menu->interactablesCount].interactableType = INTERACTABLE_TYPE_SLIDER_FLOAT;
    menu->interactablesArray[menu->interactablesCount].internalData = sliderData;

    menu->interactablesCount++;
    GRASSERT_DEBUG(menu->interactablesCount <= MAX_DBG_MENU_INTERACTABLES);
}

void DebugUIAddSliderInt(DebugMenu* menu, const char* text, i64 minValue, i64 maxValue, i64* pSliderValue)
{
    // Allocating internal slider data and saving the pointer to the slider value
    SliderIntInteractableData* sliderData = Alloc(state->interactableInternalDataAllocator, sizeof(*sliderData));
    sliderData->pSliderValue = pSliderValue;
    sliderData->minValue = minValue;
    sliderData->maxValue = maxValue;
    sliderData->valueRange = maxValue - minValue;

    // Setting the int at pSliderValue to minValue if it falls outside of min and max value, this value will be the start value of the slider and gets used to position the slider correctly.
    if (*pSliderValue < minValue || *pSliderValue > maxValue)
        *pSliderValue = minValue;

    // Percentage of how much of the slider is filled out.
    f32 sliderProgress = (f32)(*pSliderValue - minValue) / (f32)sliderData->valueRange;

    // Adding a quad for the slider bar and a quad for the slider dot.
    menu->nextElementYOffset += SLIDER_DOT_SIZE.y;
    vec2 sliderTotalPosition = vec2_create(MENU_ELEMENTS_OFFSET - SLIDER_DOT_SIZE.x / 2, menu->size.y - menu->nextElementYOffset);
    vec2 sliderTotalSize = vec2_create(SLIDER_BAR_SIZE.x + SLIDER_DOT_SIZE.x, SLIDER_DOT_SIZE.y);
    vec2 sliderBarPosition = vec2_create(MENU_ELEMENTS_OFFSET, menu->size.y - menu->nextElementYOffset + (SLIDER_DOT_SIZE.y / 4)); // Y position is slightly different than that of the dot because the origin of the quad is the bottom left corner.
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

    GRASSERT_DEBUG(menu->quadCount <= MAX_DBG_MENU_QUADS);

    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);

    menu->interactablesArray[menu->interactablesCount].firstQuad = menu->quadCount - 2; // Minus two because this function added two quads that belong to the slider.
    menu->interactablesArray[menu->interactablesCount].quadCount = 2;
    menu->interactablesArray[menu->interactablesCount].position = sliderTotalPosition;
    menu->interactablesArray[menu->interactablesCount].size = sliderTotalSize;
    menu->interactablesArray[menu->interactablesCount].interactableType = INTERACTABLE_TYPE_SLIDER_INT;
    menu->interactablesArray[menu->interactablesCount].internalData = sliderData;

    menu->interactablesCount++;
    GRASSERT_DEBUG(menu->interactablesCount <= MAX_DBG_MENU_INTERACTABLES);
}

void DebugUIAddSliderDiscrete(DebugMenu* menu, const char* text, i64* discreteValuesArray, u64 discreteValueCount, i64* pSliderValue)
{
    // Allocating internal slider data and saving the pointer to the slider value
    SliderDiscreteInteractableData* sliderData = Alloc(state->interactableInternalDataAllocator, sizeof(*sliderData));
    sliderData->pSliderValue = pSliderValue;
    sliderData->pDiscreteSliderValues = discreteValuesArray;
    sliderData->maxIndex = discreteValueCount - 1;
    // Default initialize slider index to 0, then check if the value of slider value is in the discrete values array
    // if so, set the slider index to the index of that value
    u64 sliderIndex = 0;
    for (u64 i = 0; i < discreteValueCount; i++)
    {
        if (*sliderData->pSliderValue == sliderData->pDiscreteSliderValues[i])
        {
            sliderIndex = i;
            break;
        }
    }
    // Setting the slider value to the value asigned to the current sliderIndex, this only changes the sliderValue if the initial slider value was not in the discrete values array
    *sliderData->pSliderValue = sliderData->pDiscreteSliderValues[sliderIndex];

    // Percentage of how much of the slider is filled out.
    f32 sliderProgress = (f32)sliderIndex / (f32)sliderData->maxIndex;

    // Adding a quad for the slider bar and a quad for the slider dot.
    menu->nextElementYOffset += SLIDER_DOT_SIZE.y;
    vec2 sliderTotalPosition = vec2_create(MENU_ELEMENTS_OFFSET - SLIDER_DOT_SIZE.x / 2, menu->size.y - menu->nextElementYOffset);
    vec2 sliderTotalSize = vec2_create(SLIDER_BAR_SIZE.x + SLIDER_DOT_SIZE.x, SLIDER_DOT_SIZE.y);
    vec2 sliderBarPosition = vec2_create(MENU_ELEMENTS_OFFSET, menu->size.y - menu->nextElementYOffset + (SLIDER_DOT_SIZE.y / 4)); // Y position is slightly different than that of the dot because the origin of the quad is the bottom left corner.
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

    GRASSERT_DEBUG(menu->quadCount <= MAX_DBG_MENU_QUADS);

    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);

    menu->interactablesArray[menu->interactablesCount].firstQuad = menu->quadCount - 2; // Minus two because this function added two quads that belong to the slider.
    menu->interactablesArray[menu->interactablesCount].quadCount = 2;
    menu->interactablesArray[menu->interactablesCount].position = sliderTotalPosition;
    menu->interactablesArray[menu->interactablesCount].size = sliderTotalSize;
    menu->interactablesArray[menu->interactablesCount].interactableType = INTERACTABLE_TYPE_SLIDER_DISCRETE;
    menu->interactablesArray[menu->interactablesCount].internalData = sliderData;

    menu->interactablesCount++;
    GRASSERT_DEBUG(menu->interactablesCount <= MAX_DBG_MENU_INTERACTABLES);
}

void DebugUIAddSliderLog(DebugMenu* menu, const char* text, f32 base, f32 minValue, f32 maxValue, f32* pSliderValue)
{
    f32 oneOverLogBase = 1.f / logf(base);

    // Allocating internal slider data and saving the pointer to the slider value
    SliderLogInteractableData* sliderData = Alloc(state->interactableInternalDataAllocator, sizeof(*sliderData));
    sliderData->pSliderValue = pSliderValue;
    sliderData->base = base;
    GRASSERT_DEBUG(base - nearbyintf(base) < 0.000001 || nearbyintf(base) - base < 0.000001);
    sliderData->minExponentValue = logf(minValue) * oneOverLogBase;
    sliderData->maxExponentValue = logf(maxValue) * oneOverLogBase;
    sliderData->exponentValueRange = sliderData->maxExponentValue - sliderData->minExponentValue;

    // Setting the float at pSliderValue to minValue if it falls outside of min and max value, this value will be the start value of the slider and gets used to position the slider correctly.
    f32 sliderExponentValue = logf(*sliderData->pSliderValue) * oneOverLogBase;
    if (sliderExponentValue < sliderData->minExponentValue || sliderExponentValue > sliderData->maxExponentValue)
    {
        *pSliderValue = minValue;
        sliderExponentValue = sliderData->minExponentValue;
    }

    // Percentage of how much of the slider is filled out.
    f32 sliderProgress = (sliderExponentValue - sliderData->minExponentValue) / sliderData->exponentValueRange;

    // Adding a quad for the slider bar and a quad for the slider dot.
    menu->nextElementYOffset += SLIDER_DOT_SIZE.y;
    vec2 sliderTotalPosition = vec2_create(MENU_ELEMENTS_OFFSET - SLIDER_DOT_SIZE.x / 2, menu->size.y - menu->nextElementYOffset);
    vec2 sliderTotalSize = vec2_create(SLIDER_BAR_SIZE.x + SLIDER_DOT_SIZE.x, SLIDER_DOT_SIZE.y);
    vec2 sliderBarPosition = vec2_create(MENU_ELEMENTS_OFFSET, menu->size.y - menu->nextElementYOffset + (SLIDER_DOT_SIZE.y / 4)); // Y position is slightly different than that of the dot because the origin of the quad is the bottom left corner.
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

    GRASSERT_DEBUG(menu->quadCount <= MAX_DBG_MENU_QUADS);

    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);

    menu->interactablesArray[menu->interactablesCount].firstQuad = menu->quadCount - 2; // Minus two because this function added two quads that belong to the slider.
    menu->interactablesArray[menu->interactablesCount].quadCount = 2;
    menu->interactablesArray[menu->interactablesCount].position = sliderTotalPosition;
    menu->interactablesArray[menu->interactablesCount].size = sliderTotalSize;
    menu->interactablesArray[menu->interactablesCount].interactableType = INTERACTABLE_TYPE_SLIDER_LOG;
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

/// ============================================ Toggle Button
void HandleToggleButtonInteractionStart(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition)
{
    ToggleButtonInteractableData* buttonData = interactableData->internalData;

    // Set the button to the pressed color
    if (buttonData->pStateBool)
        menu->quadsInstanceData[interactableData->firstQuad].color = vec4_mul_f32(BUTTON_PRESSED_COLOR, GREY_OUT_FACTOR);
    else
        menu->quadsInstanceData[interactableData->firstQuad].color = vec4_mul_f32(BUTTON_BASIC_COLOR, GREY_OUT_FACTOR);

    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);
}

void HandleToggleButtonInteractionUpdate(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition)
{
}

void HandleToggleButtonInteractionEnd(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition)
{
    ToggleButtonInteractableData* buttonData = interactableData->internalData;

    // If the player was still hovering over the button when they released it toggle it's state.
    if (buttonData->pStateBool && PointInRect(vec2_add_vec2(interactableData->position, menu->position), interactableData->size, vec4_xy(mouseWorldPosition)))
    {
        *buttonData->pStateBool = !*buttonData->pStateBool;
    }

    // Changing the color of the button back to the non-pressed color
    if (buttonData->pStateBool)
        menu->quadsInstanceData[interactableData->firstQuad].color = BUTTON_PRESSED_COLOR;
    else
        menu->quadsInstanceData[interactableData->firstQuad].color = BUTTON_BASIC_COLOR;

    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);
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
}

/// ============================================= Slider Float
void HandleSliderFloatInteractionStart(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition)
{
    SliderFloatInteractableData* sliderData = interactableData->internalData;

    // Calculating the percentage of slider completion based on the mouse position. It doesn't have to be mapped from 0-1 because it is already in the world position range.
    f32 mouseSliderPosition = mouseWorldPosition.x - (menu->position.x + MENU_ELEMENTS_OFFSET /*this represents the start of the slider*/);
    if (mouseSliderPosition > SLIDER_BAR_SIZE.x /*this represents the length of the slider*/)
        mouseSliderPosition = SLIDER_BAR_SIZE.x;
    if (mouseSliderPosition < 0)
        mouseSliderPosition = 0;

    vec3 sliderDotPosition = vec3_create((MENU_ELEMENTS_OFFSET - (SLIDER_DOT_SIZE.x / 2)) + mouseSliderPosition, interactableData->position.y, 0.1f); // Slider dot gets a z position to position it in front of the slider bar.
    vec2 sliderDotSize = SLIDER_DOT_SIZE;
    mat4 sliderDotTransform = mat4_mul_mat4(mat4_3Dtranslate(sliderDotPosition), mat4_2Dscale(sliderDotSize));
    menu->quadsInstanceData[interactableData->firstQuad + 1 /*indexing into the dot quad*/].transform = sliderDotTransform;
    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);

    // Updating the slider value for the client.
    mouseSliderPosition /= SLIDER_BAR_SIZE.x; // Normalizing mouseSliderPosition (or remapping it to [0, 1])
    *sliderData->pSliderValue = sliderData->minValue + (mouseSliderPosition * sliderData->valueRange);
}

void HandleSliderFloatInteractionUpdate(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition)
{
    SliderFloatInteractableData* sliderData = interactableData->internalData;

    // Calculating the percentage of slider completion based on the mouse position. It doesn't have to be mapped from 0-1 because it is already in the world position range.
    f32 mouseSliderPosition = mouseWorldPosition.x - (menu->position.x + MENU_ELEMENTS_OFFSET /*this represents the start of the slider*/);
    if (mouseSliderPosition > SLIDER_BAR_SIZE.x /*this represents the length of the slider*/)
        mouseSliderPosition = SLIDER_BAR_SIZE.x;
    if (mouseSliderPosition < 0)
        mouseSliderPosition = 0;

    vec3 sliderDotPosition = vec3_create((MENU_ELEMENTS_OFFSET - (SLIDER_DOT_SIZE.x / 2)) + mouseSliderPosition, interactableData->position.y, 0.1f); // Slider dot gets a z position to position it in front of the slider bar.
    vec2 sliderDotSize = SLIDER_DOT_SIZE;
    mat4 sliderDotTransform = mat4_mul_mat4(mat4_3Dtranslate(sliderDotPosition), mat4_2Dscale(sliderDotSize));
    menu->quadsInstanceData[interactableData->firstQuad + 1 /*indexing into the dot quad*/].transform = sliderDotTransform;
    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);

    // Updating the slider value for the client.
    mouseSliderPosition /= SLIDER_BAR_SIZE.x; // Normalizing mouseSliderPosition (or remapping it to [0, 1])
    *sliderData->pSliderValue = sliderData->minValue + (mouseSliderPosition * sliderData->valueRange);
}

void HandleSliderFloatInteractionEnd(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition)
{
    SliderFloatInteractableData* sliderData = interactableData->internalData;

    // Calculating the percentage of slider completion based on the mouse position. It doesn't have to be mapped from 0-1 because it is already in the world position range.
    f32 mouseSliderPosition = mouseWorldPosition.x - (menu->position.x + MENU_ELEMENTS_OFFSET /*this represents the start of the slider*/);
    if (mouseSliderPosition > SLIDER_BAR_SIZE.x /*this represents the length of the slider*/)
        mouseSliderPosition = SLIDER_BAR_SIZE.x;
    if (mouseSliderPosition < 0)
        mouseSliderPosition = 0;

    vec3 sliderDotPosition = vec3_create((MENU_ELEMENTS_OFFSET - (SLIDER_DOT_SIZE.x / 2)) + mouseSliderPosition, interactableData->position.y, 0.1f); // Slider dot gets a z position to position it in front of the slider bar.
    vec2 sliderDotSize = SLIDER_DOT_SIZE;
    mat4 sliderDotTransform = mat4_mul_mat4(mat4_3Dtranslate(sliderDotPosition), mat4_2Dscale(sliderDotSize));
    menu->quadsInstanceData[interactableData->firstQuad + 1 /*indexing into the dot quad*/].transform = sliderDotTransform;
    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);

    // Updating the slider value for the client.
    mouseSliderPosition /= SLIDER_BAR_SIZE.x; // Normalizing mouseSliderPosition (or remapping it to [0, 1])
    *sliderData->pSliderValue = sliderData->minValue + (mouseSliderPosition * sliderData->valueRange);
}

/// ============================================= Slider Int
void HandleSliderIntInteractionStart(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition)
{
    SliderIntInteractableData* sliderData = interactableData->internalData;

    // Calculating the percentage of slider completion based on the mouse position. It doesn't have to be mapped from 0-1 because it is already in the world position range.
    f32 mouseSliderPosition = mouseWorldPosition.x - (menu->position.x + MENU_ELEMENTS_OFFSET /*this represents the start of the slider*/);
    if (mouseSliderPosition > SLIDER_BAR_SIZE.x /*this represents the length of the slider*/)
        mouseSliderPosition = SLIDER_BAR_SIZE.x;
    if (mouseSliderPosition < 0)
        mouseSliderPosition = 0;

    mouseSliderPosition /= SLIDER_BAR_SIZE.x;                                                            // Normalizing mouseSliderPosition (or remapping it to [0, 1])
    mouseSliderPosition *= (f32)sliderData->valueRange;                                                  // Mapping the mouseSliderPosition from 0 to valueRange
    mouseSliderPosition = nearbyintf(mouseSliderPosition);                                               // Rounding mouseSliderPosition to the nearest float representable integer
    f32 roundedSliderPosition = SLIDER_BAR_SIZE.x * (mouseSliderPosition / (f32)sliderData->valueRange); // Mapping the range back to 0-1 and then to 0-slider_bar_size_x

    vec3 sliderDotPosition = vec3_create((MENU_ELEMENTS_OFFSET - (SLIDER_DOT_SIZE.x / 2)) + roundedSliderPosition, interactableData->position.y, 0.1f); // Slider dot gets a z position to position it in front of the slider bar.
    vec2 sliderDotSize = SLIDER_DOT_SIZE;
    mat4 sliderDotTransform = mat4_mul_mat4(mat4_3Dtranslate(sliderDotPosition), mat4_2Dscale(sliderDotSize));
    menu->quadsInstanceData[interactableData->firstQuad + 1 /*indexing into the dot quad*/].transform = sliderDotTransform;
    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);

    // Updating the slider value for the client.
    *sliderData->pSliderValue = sliderData->minValue + (i64)mouseSliderPosition;
}

void HandleSliderIntInteractionUpdate(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition)
{
    SliderIntInteractableData* sliderData = interactableData->internalData;

    // Calculating the percentage of slider completion based on the mouse position. It doesn't have to be mapped from 0-1 because it is already in the world position range.
    f32 mouseSliderPosition = mouseWorldPosition.x - (menu->position.x + MENU_ELEMENTS_OFFSET /*this represents the start of the slider*/);
    if (mouseSliderPosition > SLIDER_BAR_SIZE.x /*this represents the length of the slider*/)
        mouseSliderPosition = SLIDER_BAR_SIZE.x;
    if (mouseSliderPosition < 0)
        mouseSliderPosition = 0;

    mouseSliderPosition /= SLIDER_BAR_SIZE.x;                                                            // Normalizing mouseSliderPosition (or remapping it to [0, 1])
    mouseSliderPosition *= (f32)sliderData->valueRange;                                                  // Mapping the mouseSliderPosition from 0 to valueRange
    mouseSliderPosition = nearbyintf(mouseSliderPosition);                                               // Rounding mouseSliderPosition to the nearest float representable integer
    f32 roundedSliderPosition = SLIDER_BAR_SIZE.x * (mouseSliderPosition / (f32)sliderData->valueRange); // Mapping the range back to 0-1 and then to 0-slider_bar_size_x

    vec3 sliderDotPosition = vec3_create((MENU_ELEMENTS_OFFSET - (SLIDER_DOT_SIZE.x / 2)) + roundedSliderPosition, interactableData->position.y, 0.1f); // Slider dot gets a z position to position it in front of the slider bar.
    vec2 sliderDotSize = SLIDER_DOT_SIZE;
    mat4 sliderDotTransform = mat4_mul_mat4(mat4_3Dtranslate(sliderDotPosition), mat4_2Dscale(sliderDotSize));
    menu->quadsInstanceData[interactableData->firstQuad + 1 /*indexing into the dot quad*/].transform = sliderDotTransform;
    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);

    // Updating the slider value for the client.
    *sliderData->pSliderValue = sliderData->minValue + (i64)mouseSliderPosition;
}

void HandleSliderIntInteractionEnd(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition)
{
    SliderIntInteractableData* sliderData = interactableData->internalData;

    // Calculating the percentage of slider completion based on the mouse position. It doesn't have to be mapped from 0-1 because it is already in the world position range.
    f32 mouseSliderPosition = mouseWorldPosition.x - (menu->position.x + MENU_ELEMENTS_OFFSET /*this represents the start of the slider*/);
    if (mouseSliderPosition > SLIDER_BAR_SIZE.x /*this represents the length of the slider*/)
        mouseSliderPosition = SLIDER_BAR_SIZE.x;
    if (mouseSliderPosition < 0)
        mouseSliderPosition = 0;

    mouseSliderPosition /= SLIDER_BAR_SIZE.x;                                                            // Normalizing mouseSliderPosition (or remapping it to [0, 1])
    mouseSliderPosition *= (f32)sliderData->valueRange;                                                  // Mapping the mouseSliderPosition from 0 to valueRange
    mouseSliderPosition = nearbyintf(mouseSliderPosition);                                               // Rounding mouseSliderPosition to the nearest float representable integer
    f32 roundedSliderPosition = SLIDER_BAR_SIZE.x * (mouseSliderPosition / (f32)sliderData->valueRange); // Mapping the range back to 0-1 and then to 0-slider_bar_size_x

    vec3 sliderDotPosition = vec3_create((MENU_ELEMENTS_OFFSET - (SLIDER_DOT_SIZE.x / 2)) + roundedSliderPosition, interactableData->position.y, 0.1f); // Slider dot gets a z position to position it in front of the slider bar.
    vec2 sliderDotSize = SLIDER_DOT_SIZE;
    mat4 sliderDotTransform = mat4_mul_mat4(mat4_3Dtranslate(sliderDotPosition), mat4_2Dscale(sliderDotSize));
    menu->quadsInstanceData[interactableData->firstQuad + 1 /*indexing into the dot quad*/].transform = sliderDotTransform;
    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);

    // Updating the slider value for the client.
    *sliderData->pSliderValue = sliderData->minValue + (i64)mouseSliderPosition;
}

/// ============================================= Slider Discrete
void HandleSliderDiscreteInteractionStart(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition)
{
    SliderDiscreteInteractableData* sliderData = interactableData->internalData;

    // Calculating the percentage of slider completion based on the mouse position. It doesn't have to be mapped from 0-1 because it is already in the world position range.
    f32 mouseSliderPosition = mouseWorldPosition.x - (menu->position.x + MENU_ELEMENTS_OFFSET /*this represents the start of the slider*/);
    if (mouseSliderPosition > SLIDER_BAR_SIZE.x /*this represents the length of the slider*/)
        mouseSliderPosition = SLIDER_BAR_SIZE.x;
    if (mouseSliderPosition < 0)
        mouseSliderPosition = 0;

    mouseSliderPosition /= SLIDER_BAR_SIZE.x;                                                          // Normalizing mouseSliderPosition (or remapping it to [0, 1])
    mouseSliderPosition *= (f32)sliderData->maxIndex;                                                  // Mapping the mouseSliderPosition from 0 to maxIndex
    mouseSliderPosition = nearbyintf(mouseSliderPosition);                                             // Rounding mouseSliderPosition to the nearest float representable integer
    f32 roundedSliderPosition = SLIDER_BAR_SIZE.x * (mouseSliderPosition / (f32)sliderData->maxIndex); // Mapping the range back to 0-1 and then to 0-slider_bar_size_x

    vec3 sliderDotPosition = vec3_create((MENU_ELEMENTS_OFFSET - (SLIDER_DOT_SIZE.x / 2)) + roundedSliderPosition, interactableData->position.y, 0.1f); // Slider dot gets a z position to position it in front of the slider bar.
    vec2 sliderDotSize = SLIDER_DOT_SIZE;
    mat4 sliderDotTransform = mat4_mul_mat4(mat4_3Dtranslate(sliderDotPosition), mat4_2Dscale(sliderDotSize));
    menu->quadsInstanceData[interactableData->firstQuad + 1 /*indexing into the dot quad*/].transform = sliderDotTransform;
    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);

    // Updating the slider value for the client.
    *sliderData->pSliderValue = sliderData->pDiscreteSliderValues[(u64)mouseSliderPosition];
}

void HandleSliderDiscreteInteractionUpdate(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition)
{
    SliderDiscreteInteractableData* sliderData = interactableData->internalData;

    // Calculating the percentage of slider completion based on the mouse position. It doesn't have to be mapped from 0-1 because it is already in the world position range.
    f32 mouseSliderPosition = mouseWorldPosition.x - (menu->position.x + MENU_ELEMENTS_OFFSET /*this represents the start of the slider*/);
    if (mouseSliderPosition > SLIDER_BAR_SIZE.x /*this represents the length of the slider*/)
        mouseSliderPosition = SLIDER_BAR_SIZE.x;
    if (mouseSliderPosition < 0)
        mouseSliderPosition = 0;

    mouseSliderPosition /= SLIDER_BAR_SIZE.x;                                                          // Normalizing mouseSliderPosition (or remapping it to [0, 1])
    mouseSliderPosition *= (f32)sliderData->maxIndex;                                                  // Mapping the mouseSliderPosition from 0 to maxIndex
    mouseSliderPosition = nearbyintf(mouseSliderPosition);                                             // Rounding mouseSliderPosition to the nearest float representable integer
    f32 roundedSliderPosition = SLIDER_BAR_SIZE.x * (mouseSliderPosition / (f32)sliderData->maxIndex); // Mapping the range back to 0-1 and then to 0-slider_bar_size_x

    vec3 sliderDotPosition = vec3_create((MENU_ELEMENTS_OFFSET - (SLIDER_DOT_SIZE.x / 2)) + roundedSliderPosition, interactableData->position.y, 0.1f); // Slider dot gets a z position to position it in front of the slider bar.
    vec2 sliderDotSize = SLIDER_DOT_SIZE;
    mat4 sliderDotTransform = mat4_mul_mat4(mat4_3Dtranslate(sliderDotPosition), mat4_2Dscale(sliderDotSize));
    menu->quadsInstanceData[interactableData->firstQuad + 1 /*indexing into the dot quad*/].transform = sliderDotTransform;
    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);

    // Updating the slider value for the client.
    *sliderData->pSliderValue = sliderData->pDiscreteSliderValues[(u64)mouseSliderPosition];
}

void HandleSliderDiscreteInteractionEnd(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition)
{
    SliderDiscreteInteractableData* sliderData = interactableData->internalData;

    // Calculating the percentage of slider completion based on the mouse position. It doesn't have to be mapped from 0-1 because it is already in the world position range.
    f32 mouseSliderPosition = mouseWorldPosition.x - (menu->position.x + MENU_ELEMENTS_OFFSET /*this represents the start of the slider*/);
    if (mouseSliderPosition > SLIDER_BAR_SIZE.x /*this represents the length of the slider*/)
        mouseSliderPosition = SLIDER_BAR_SIZE.x;
    if (mouseSliderPosition < 0)
        mouseSliderPosition = 0;

    mouseSliderPosition /= SLIDER_BAR_SIZE.x;                                                          // Normalizing mouseSliderPosition (or remapping it to [0, 1])
    mouseSliderPosition *= (f32)sliderData->maxIndex;                                                  // Mapping the mouseSliderPosition from 0 to maxIndex
    mouseSliderPosition = nearbyintf(mouseSliderPosition);                                             // Rounding mouseSliderPosition to the nearest float representable integer
    f32 roundedSliderPosition = SLIDER_BAR_SIZE.x * (mouseSliderPosition / (f32)sliderData->maxIndex); // Mapping the range back to 0-1 and then to 0-slider_bar_size_x

    vec3 sliderDotPosition = vec3_create((MENU_ELEMENTS_OFFSET - (SLIDER_DOT_SIZE.x / 2)) + roundedSliderPosition, interactableData->position.y, 0.1f); // Slider dot gets a z position to position it in front of the slider bar.
    vec2 sliderDotSize = SLIDER_DOT_SIZE;
    mat4 sliderDotTransform = mat4_mul_mat4(mat4_3Dtranslate(sliderDotPosition), mat4_2Dscale(sliderDotSize));
    menu->quadsInstanceData[interactableData->firstQuad + 1 /*indexing into the dot quad*/].transform = sliderDotTransform;
    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);

    // Updating the slider value for the client.
    *sliderData->pSliderValue = sliderData->pDiscreteSliderValues[(u64)mouseSliderPosition];
}

/// ============================================= Slider Log
void HandleSliderLogInteractionStart(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition)
{
    SliderLogInteractableData* sliderData = interactableData->internalData;

    // Calculating the percentage of slider completion based on the mouse position. It doesn't have to be mapped from 0-1 because it is already in the world position range.
    f32 mouseSliderPosition = mouseWorldPosition.x - (menu->position.x + MENU_ELEMENTS_OFFSET /*this represents the start of the slider*/);
    if (mouseSliderPosition > SLIDER_BAR_SIZE.x /*this represents the length of the slider*/)
        mouseSliderPosition = SLIDER_BAR_SIZE.x;
    if (mouseSliderPosition < 0)
        mouseSliderPosition = 0;

    vec3 sliderDotPosition = vec3_create((MENU_ELEMENTS_OFFSET - (SLIDER_DOT_SIZE.x / 2)) + mouseSliderPosition, interactableData->position.y, 0.1f); // Slider dot gets a z position to position it in front of the slider bar.
    vec2 sliderDotSize = SLIDER_DOT_SIZE;
    mat4 sliderDotTransform = mat4_mul_mat4(mat4_3Dtranslate(sliderDotPosition), mat4_2Dscale(sliderDotSize));
    menu->quadsInstanceData[interactableData->firstQuad + 1 /*indexing into the dot quad*/].transform = sliderDotTransform;
    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);

    // Updating the slider value for the client.
    mouseSliderPosition /= SLIDER_BAR_SIZE.x; // Normalizing mouseSliderPosition (or remapping it to [0, 1])
    *sliderData->pSliderValue = powf(sliderData->base, sliderData->minExponentValue + (mouseSliderPosition * sliderData->exponentValueRange));
}

void HandleSliderLogInteractionUpdate(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition)
{
    SliderLogInteractableData* sliderData = interactableData->internalData;

    // Calculating the percentage of slider completion based on the mouse position. It doesn't have to be mapped from 0-1 because it is already in the world position range.
    f32 mouseSliderPosition = mouseWorldPosition.x - (menu->position.x + MENU_ELEMENTS_OFFSET /*this represents the start of the slider*/);
    if (mouseSliderPosition > SLIDER_BAR_SIZE.x /*this represents the length of the slider*/)
        mouseSliderPosition = SLIDER_BAR_SIZE.x;
    if (mouseSliderPosition < 0)
        mouseSliderPosition = 0;

    vec3 sliderDotPosition = vec3_create((MENU_ELEMENTS_OFFSET - (SLIDER_DOT_SIZE.x / 2)) + mouseSliderPosition, interactableData->position.y, 0.1f); // Slider dot gets a z position to position it in front of the slider bar.
    vec2 sliderDotSize = SLIDER_DOT_SIZE;
    mat4 sliderDotTransform = mat4_mul_mat4(mat4_3Dtranslate(sliderDotPosition), mat4_2Dscale(sliderDotSize));
    menu->quadsInstanceData[interactableData->firstQuad + 1 /*indexing into the dot quad*/].transform = sliderDotTransform;
    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);

    // Updating the slider value for the client.
    mouseSliderPosition /= SLIDER_BAR_SIZE.x; // Normalizing mouseSliderPosition (or remapping it to [0, 1])
    *sliderData->pSliderValue = powf(sliderData->base, sliderData->minExponentValue + (mouseSliderPosition * sliderData->exponentValueRange));
}

void HandleSliderLogInteractionEnd(DebugMenu* menu, InteractableData* interactableData, vec4 mouseWorldPosition)
{
    SliderLogInteractableData* sliderData = interactableData->internalData;

    // Calculating the percentage of slider completion based on the mouse position. It doesn't have to be mapped from 0-1 because it is already in the world position range.
    f32 mouseSliderPosition = mouseWorldPosition.x - (menu->position.x + MENU_ELEMENTS_OFFSET /*this represents the start of the slider*/);
    if (mouseSliderPosition > SLIDER_BAR_SIZE.x /*this represents the length of the slider*/)
        mouseSliderPosition = SLIDER_BAR_SIZE.x;
    if (mouseSliderPosition < 0)
        mouseSliderPosition = 0;

    vec3 sliderDotPosition = vec3_create((MENU_ELEMENTS_OFFSET - (SLIDER_DOT_SIZE.x / 2)) + mouseSliderPosition, interactableData->position.y, 0.1f); // Slider dot gets a z position to position it in front of the slider bar.
    vec2 sliderDotSize = SLIDER_DOT_SIZE;
    mat4 sliderDotTransform = mat4_mul_mat4(mat4_3Dtranslate(sliderDotPosition), mat4_2Dscale(sliderDotSize));
    menu->quadsInstanceData[interactableData->firstQuad + 1 /*indexing into the dot quad*/].transform = sliderDotTransform;
    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(*menu->quadsInstanceData) * menu->quadCount);

    // Updating the slider value for the client.
    mouseSliderPosition /= SLIDER_BAR_SIZE.x; // Normalizing mouseSliderPosition (or remapping it to [0, 1])
    *sliderData->pSliderValue = powf(sliderData->base, sliderData->minExponentValue + (mouseSliderPosition * sliderData->exponentValueRange));
}
