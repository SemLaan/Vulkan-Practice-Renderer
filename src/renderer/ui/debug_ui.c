#include "debug_ui.h"

#include "core/asserts.h"
#include "core/event.h"
#include "core/meminc.h"
#include "core/platform.h"
#include "core/input.h"
#include "math/lin_alg.h"
#include "renderer/renderer.h"
#include "renderer/ui/text_renderer.h"


typedef struct DebugMenu
{
    mat4 menuTransform;           // Transform of the menu.
    vec2 position;                // Position, anchor is bottom left of the menu.
    vec2 size;                    // Vec 2 with the menu size, x is width y is height.
    mat4* instanceData;           // Instance data on the CPU
    VertexBuffer instancedQuadVB; // Vertex buffer with model matrices for quads
} DebugMenu;

typedef struct DebugUIState
{
    DebugMenu** debugMenuDarray;
    Material roundedQuadMaterial;
    MeshData* quadMesh;
    mat4 uiProjView;
    mat4 inverseProjView;
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
    shaderCreateInfo.vertexBufferLayout.perInstanceAttributeCount = 1;
    shaderCreateInfo.vertexBufferLayout.perInstanceAttributes[0] = VERTEX_ATTRIBUTE_TYPE_MAT4;

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

        if (GetButtonDown(BUTTON_LEFTMOUSEBTN) && !GetButtonDownPrevious(BUTTON_LEFTMOUSEBTN))
        {
            vec4 mouseScreenPos = vec4_create(GetMousePos().x, GetMousePos().y, 0, 1);

			//_DEBUG("clip: x: %.2f, y: %.2f, z: %.2f", mouseScreenPos.x, mouseScreenPos.y, 0.f);

			vec4 clipCoords = ScreenToClipSpace(mouseScreenPos);
            vec4 mouseWorldPos = mat4_mul_vec4(state->inverseProjView, clipCoords);
            //_DEBUG("world: x: %.2f, y: %.2f, z: %.2f", mouseWorldPos.x, mouseWorldPos.y, mouseWorldPos.z);

            vec4 test = mat4_mul_vec4(state->uiProjView, mouseWorldPos);
            //_DEBUG("clip test:  x: %.2f, y: %.2f, z: %.2f", test.x, test.y, test.z);

			bool mouseInMenu = PointInRect(menu->position, menu->size, vec4_xy(mouseWorldPos));
			_DEBUG("Menu clicked: %s", mouseInMenu ? "true" : "false");
        }
    }
}

DebugMenu* DebugUICreateMenu()
{
    DebugMenu* menu = Alloc(GetGlobalAllocator(), sizeof(*menu), MEM_TAG_RENDERER_SUBSYS);

    menu->position = vec2_create(1, 1);
    menu->size = vec2_create(1, 1);
    mat4 rotate = mat4_rotate_x(0);
    menu->menuTransform = mat4_mul_mat4(mat4_2Dtranslate(menu->position), mat4_mul_mat4(rotate, mat4_2Dscale(menu->size)));

    menu->instanceData = Alloc(GetGlobalAllocator(), sizeof(mat4) * 1, MEM_TAG_RENDERER_SUBSYS);
    menu->instanceData[0] = menu->menuTransform;

    menu->instancedQuadVB = VertexBufferCreate(menu->instanceData, sizeof(mat4) * 1);

    state->debugMenuDarray = DarrayPushback(state->debugMenuDarray, &menu);

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

    VertexBuffer vertexBuffers[2] = {state->quadMesh->vertexBuffer, menu->instancedQuadVB};

    Draw(2, vertexBuffers, state->quadMesh->indexBuffer, nullptr, 1);
}
