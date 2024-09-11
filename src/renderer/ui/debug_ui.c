#include "debug_ui.h"

#include "core/asserts.h"
#include "core/event.h"
#include "core/input.h"
#include "core/meminc.h"
#include "core/platform.h"
#include "math/lin_alg.h"
#include "renderer/renderer.h"
#include "renderer/ui/text_renderer.h"

#define MAX_DBG_MENU_QUADS 100
#define MAX_DBG_MENU_INTERACTABLES 20

typedef struct QuadInstanceData
{
    mat4 transform;
    vec4 color;
} QuadInstanceData;

typedef struct InteractableData
{
    vec2 position;
    vec2 size;
    u32 firstQuad;
    u32 quadCount;
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
    i32 activeInteractable;               // Index into interactables array, is -1 if no interactable is being interacted with. (If a button is being pressed or a slider is being dragged, this will indicate that.)
    u32 maxQuads;                         // Max amount of quads
    u32 quadCount;                        // Current amount of quads
    u32 interactablesCount;               // Amount of buttons, sliders or other elements in the menu.
} DebugMenu;

// Data about the state of the Debug ui system, only one instance of this struct should exist.
typedef struct DebugUIState
{
    DebugMenu** debugMenuDarray;  // Dynamic array with all the debug menu instances that exist
    Material roundedQuadMaterial; // Material to render quads with (may not actually create rounded quads yet).
    MeshData* quadMesh;           // Mesh with the data to make quad instances.
    mat4 uiProjView;              // Projection and view matrix for all debug menu's
    mat4 inverseProjView;         // Inverted proj view matrix.
} DebugUIState;

static DebugUIState* state = nullptr;

static bool OnWindowResize(EventCode type, EventData data)
{
    vec2i windowSize = GetPlatformWindowSize();
    float windowAspectRatio = windowSize.x / (float)windowSize.y;
    state->uiProjView = mat4_orthographic(0, 10 * windowAspectRatio, 0, 10, -1, 1);
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

    // Creating the quad shader and material
    ShaderCreateInfo shaderCreateInfo = {};
    shaderCreateInfo.renderTargetStencil = false;
    shaderCreateInfo.renderTargetDepth = true;
    shaderCreateInfo.renderTargetColor = true;
    shaderCreateInfo.vertexShaderName = "roundedQuad";
    shaderCreateInfo.fragmentShaderName = "roundedQuad";
    shaderCreateInfo.vertexBufferLayout.perVertexAttributeCount = 3;
    shaderCreateInfo.vertexBufferLayout.perVertexAttributes[0] = VERTEX_ATTRIBUTE_TYPE_VEC3;
    shaderCreateInfo.vertexBufferLayout.perVertexAttributes[1] = VERTEX_ATTRIBUTE_TYPE_VEC3;
    shaderCreateInfo.vertexBufferLayout.perVertexAttributes[2] = VERTEX_ATTRIBUTE_TYPE_VEC2;
    shaderCreateInfo.vertexBufferLayout.perInstanceAttributeCount = 2;
    shaderCreateInfo.vertexBufferLayout.perInstanceAttributes[0] = VERTEX_ATTRIBUTE_TYPE_MAT4;
    shaderCreateInfo.vertexBufferLayout.perInstanceAttributes[1] = VERTEX_ATTRIBUTE_TYPE_VEC4;

    ShaderCreate("roundedQuad", &shaderCreateInfo);
    state->roundedQuadMaterial = MaterialCreate(ShaderGetRef("roundedQuad"));

    state->debugMenuDarray = DarrayCreate(sizeof(*state->debugMenuDarray), 2, GetGlobalAllocator(), MEM_TAG_RENDERER_SUBSYS);

    state->quadMesh = GetBasicMesh(BASIC_MESH_NAME_QUAD);

    vec2i windowSize = GetPlatformWindowSize();
    float windowAspectRatio = windowSize.x / (float)windowSize.y;
    mat4 projection = mat4_orthographic(0, 10 * windowAspectRatio, 0, 10, -1, 1);
    mat4 view = mat4_3Dtranslate(vec3_create(0, 0, 0));
    state->uiProjView = mat4_mul_mat4(projection, view);
    state->inverseProjView = mat4_inverse(state->uiProjView);

    RegisterEventListener(EVCODE_WINDOW_RESIZED, OnWindowResize);

    return true;
}

void ShutdownDebugUI()
{
    UnregisterEventListener(EVCODE_WINDOW_RESIZED, OnWindowResize);

    MaterialDestroy(state->roundedQuadMaterial);
    DarrayDestroy(state->debugMenuDarray);
    Free(GetGlobalAllocator(), state);
}

void UpdateDebugUI()
{
    u32 menuCount = DarrayGetSize(state->debugMenuDarray);

    for (int i = 0; i < menuCount; i++)
    {
        DebugMenu* menu = state->debugMenuDarray[i];

		if (menu->activeInteractable != -1) // If a button or slider is being interacted with already
		{
			if (!GetButtonDown(BUTTON_LEFTMOUSEBTN))
			{
				menu->quadsInstanceData[menu->interactablesArray[menu->activeInteractable].firstQuad].color	= vec4_create(1.0f, 0.0f, 0.0f, 1.0f);
				VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(mat4) * menu->quadCount);
				menu->activeInteractable = -1;
			}
		}
        else if (GetButtonDown(BUTTON_LEFTMOUSEBTN) && !GetButtonDownPrevious(BUTTON_LEFTMOUSEBTN)) // If NO interactable in this menu is being interacted with and the button was pressed.
        {
            vec4 mouseScreenPos = vec4_create(GetMousePos().x, GetMousePos().y, 0, 1);

            vec4 clipCoords = ScreenToClipSpace(mouseScreenPos);
            vec4 mouseWorldPos = mat4_mul_vec4(state->inverseProjView, clipCoords);

            bool mouseInMenu = PointInRect(menu->position, menu->size, vec4_xy(mouseWorldPos));
            _DEBUG("Menu clicked: %s", mouseInMenu ? "true" : "false");

            if (mouseInMenu)
            {
                // TODO: remove this as it is only for testing changing color of rects.
                for (int j = 0; j < menu->interactablesCount; j++)
                {
                    menu->quadsInstanceData[menu->interactablesArray[j].firstQuad].color = vec4_create(0.0f, 0.0f, 1.0f, 1.0f);
                    menu->activeInteractable = j;
                }

                VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(mat4) * menu->quadCount);
            }
        }
    }
}

DebugMenu* DebugUICreateMenu()
{
    DebugMenu* menu = Alloc(GetGlobalAllocator(), sizeof(*menu), MEM_TAG_RENDERER_SUBSYS);

    menu->position = vec2_create(0.3f, 0.3f);
    menu->size = vec2_create(2.5f, 9.4f);
    mat4 rotate = mat4_rotate_x(0);
    menu->menuTransform = mat4_mul_mat4(mat4_2Dtranslate(menu->position), mat4_mul_mat4(rotate, mat4_2Dscale(menu->size)));

    menu->quadsInstanceData = Alloc(GetGlobalAllocator(), sizeof(*menu->quadsInstanceData) * MAX_DBG_MENU_QUADS, MEM_TAG_RENDERER_SUBSYS);
    menu->quadsInstanceData[0].transform = menu->menuTransform;
    menu->quadsInstanceData[0].color = vec4_create(1, 1, 1, 1);
    menu->maxQuads = MAX_DBG_MENU_QUADS;
    menu->quadCount = 1;

    menu->quadsInstancedVB = VertexBufferCreate(menu->quadsInstanceData, sizeof(mat4) * MAX_DBG_MENU_QUADS);

    state->debugMenuDarray = DarrayPushback(state->debugMenuDarray, &menu);

    menu->interactablesArray = Alloc(GetGlobalAllocator(), sizeof(*menu->interactablesArray) * MAX_DBG_MENU_INTERACTABLES, MEM_TAG_RENDERER_SUBSYS);
    menu->interactablesCount = 0;
	menu->activeInteractable = -1;

    return menu;
}

void DebugUIDestroyMenu(DebugMenu* menu)
{
    // TODO:
}

void DebugUIRenderMenu(DebugMenu* menu)
{
    MaterialUpdateProperty(state->roundedQuadMaterial, "menuView", &state->uiProjView);
    MaterialBind(state->roundedQuadMaterial);

    VertexBuffer vertexBuffers[2] = {state->quadMesh->vertexBuffer, menu->quadsInstancedVB};

    Draw(2, vertexBuffers, state->quadMesh->indexBuffer, nullptr, menu->quadCount);
}

void DebugUIAddButton(DebugMenu* menu, const char* text, bool* boolPointer)
{
    vec2 buttonPosition = vec2_create(3, 0.3f);
    vec2 buttonSize = vec2_create(12.5f, 9.4f);
    mat4 rotate = mat4_rotate_x(0);
    mat4 buttonTransform = mat4_mul_mat4(mat4_2Dtranslate(buttonPosition), mat4_mul_mat4(rotate, mat4_2Dscale(buttonSize)));
    menu->quadsInstanceData[menu->quadCount].transform = buttonTransform;
    menu->quadsInstanceData[menu->quadCount].color = vec4_create(1, 0, 0, 1);
    menu->quadCount++;

    GRASSERT_DEBUG(menu->quadCount <= MAX_DBG_MENU_QUADS);

    VertexBufferUpdate(menu->quadsInstancedVB, menu->quadsInstanceData, sizeof(mat4) * menu->quadCount);

    menu->interactablesArray[menu->interactablesCount].firstQuad = menu->quadCount - 1;
    menu->interactablesArray[menu->interactablesCount].quadCount = 1;
    menu->interactablesArray[menu->interactablesCount].position = buttonPosition;
    menu->interactablesArray[menu->interactablesCount].size = buttonSize;

    menu->interactablesCount++;
    GRASSERT_DEBUG(menu->interactablesCount <= MAX_DBG_MENU_INTERACTABLES);
}
