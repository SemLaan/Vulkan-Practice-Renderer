#include "game_rendering.h"

#include "core/event.h"
#include "core/platform.h"
#include "marching_cubes.h"
#include "math/lin_alg.h"
#include "renderer/render_target.h"
#include "renderer/renderer.h"
#include "renderer/ui/text_renderer.h"

#define MARCHING_CUBES_SHADER_NAME "marchingCubes"
#define NORMAL_SHADER_NAME "normal_shader"
#define FONT_NAME_ROBOTO "roboto"
#define FONT_NAME_ADORABLE_HANDMADE "adorable"
#define FONT_NAME_NICOLAST "nicolast"
#define DEFAULT_FOV 45.0f
#define DEFAULT_NEAR_PLANE 1.0f
#define DEFAULT_FAR_PLANE 200.0f
#define UI_NEAR_PLANE -1
#define UI_FAR_PLANE 1
#define UI_ORTHO_HEIGHT 10

DEFINE_DARRAY_TYPE_REF(DebugMenu);

typedef struct GameRenderingState
{
    // Debug menu's
    DebugMenuRefDarray* debugMenuDarray;

    // Materials
    Material marchingCubesMaterial;
	Material normalRenderingMaterial;

    // Render targets

    // Camera matrices and positions
    Camera sceneCamera;
    Camera uiCamera;

    // Text, THIS IS TEMPORARY, this needs to be changed once the text system is finished
    Text* textTest;
} GameRenderingState;

static GameRenderingState* renderingState = nullptr;

static bool OnWindowResize(EventCode type, EventData data)
{
    vec2i windowSize = GetPlatformWindowSize();
    float windowAspectRatio = windowSize.x / (float)windowSize.y;
    renderingState->sceneCamera.projection = mat4_perspective(DEFAULT_FOV, windowAspectRatio, DEFAULT_NEAR_PLANE, DEFAULT_FAR_PLANE);
    renderingState->uiCamera.projection = mat4_orthographic(0, UI_ORTHO_HEIGHT * windowAspectRatio, 0, UI_ORTHO_HEIGHT, UI_NEAR_PLANE, UI_FAR_PLANE);

    return false;
}

void GameRenderingInit()
{
    renderingState = Alloc(GetGlobalAllocator(), sizeof(*renderingState), MEM_TAG_GAME);

    // Calculating camera projections and listening to the window resize event to recalculate projection on window resize
    vec2i windowSize = GetPlatformWindowSize();
    float windowAspectRatio = windowSize.x / (float)windowSize.y;
    renderingState->sceneCamera.projection = mat4_perspective(DEFAULT_FOV, windowAspectRatio, DEFAULT_NEAR_PLANE, DEFAULT_FAR_PLANE);
    renderingState->uiCamera.projection = mat4_orthographic(0, UI_ORTHO_HEIGHT * windowAspectRatio, 0, UI_ORTHO_HEIGHT, UI_NEAR_PLANE, UI_FAR_PLANE);

    RegisterEventListener(EVCODE_WINDOW_RESIZED, OnWindowResize);

    renderingState->sceneCamera.position = vec3_create(0, 0, 0);
    renderingState->sceneCamera.rotation = vec3_create(0, 0, 0);
    renderingState->uiCamera.position = vec3_create(0, 0, 0);
    renderingState->uiCamera.rotation = vec3_create(0, 0, 0);

    // Creating darray for debug ui's
    renderingState->debugMenuDarray = DebugMenuRefDarrayCreate(5, GetGlobalAllocator());

    // Loading fonts
    TextLoadFont(FONT_NAME_ROBOTO, "Roboto-Black.ttf");
    TextLoadFont(FONT_NAME_ADORABLE_HANDMADE, "Adorable Handmade.ttf");
    TextLoadFont(FONT_NAME_NICOLAST, "Nicolast.ttf");

    // Creating test text
    // TODO: this will be replaced once the text rendering system is finished
    const char* testString = "Beefy text testing!?.";
    renderingState->textTest = TextCreate(testString, FONT_NAME_ROBOTO, renderingState->uiCamera.projection, UPDATE_FREQUENCY_STATIC);

    // Loading shaders
    {
        ShaderCreateInfo shaderCreateInfo = {};
        shaderCreateInfo.renderTargetStencil = false;
        shaderCreateInfo.vertexBufferLayout.perVertexAttributeCount = 2;
        shaderCreateInfo.vertexBufferLayout.perVertexAttributes[0] = VERTEX_ATTRIBUTE_TYPE_VEC3;
        shaderCreateInfo.vertexBufferLayout.perVertexAttributes[1] = VERTEX_ATTRIBUTE_TYPE_VEC3;
        shaderCreateInfo.vertexBufferLayout.perInstanceAttributeCount = 0;

        shaderCreateInfo.vertexShaderName = "marchingCubes";
        shaderCreateInfo.fragmentShaderName = "marchingCubes";
        shaderCreateInfo.renderTargetColor = true;
        shaderCreateInfo.renderTargetDepth = true;
        ShaderCreate(MARCHING_CUBES_SHADER_NAME, &shaderCreateInfo);

		shaderCreateInfo.vertexShaderName = "normal";
        shaderCreateInfo.fragmentShaderName = "normal";
		ShaderCreate(NORMAL_SHADER_NAME, &shaderCreateInfo);
    }

    // Creating materials
    {
        renderingState->marchingCubesMaterial = MaterialCreate(ShaderGetRef(MARCHING_CUBES_SHADER_NAME));
		renderingState->normalRenderingMaterial = MaterialCreate(ShaderGetRef(NORMAL_SHADER_NAME));
    }

    // Initializing material state
    {
        vec4 testColor = vec4_create(0.2, 0.4f, 1, 1);
        f32 roughness = 0;
        MaterialUpdateProperty(renderingState->marchingCubesMaterial, "color", &testColor);
        MaterialUpdateProperty(renderingState->marchingCubesMaterial, "roughness", &roughness);
    }

    MCGenerateDensityMap();
    MCGenerateMesh();
}

void GameRenderingRender()
{
    vec4 testColor = vec4_create(0.2, 0.4f, 1, 1);
    f32 roughness = 0;
    MaterialUpdateProperty(renderingState->marchingCubesMaterial, "color", &testColor);
    MaterialUpdateProperty(renderingState->marchingCubesMaterial, "roughness", &roughness);

    // ================== Camera calculations
    CameraRecalculateViewAndViewProjection(&renderingState->sceneCamera);

    GlobalUniformObject globalUniformObject = {};
    globalUniformObject.viewPosition = vec3_invert_sign(renderingState->sceneCamera.position);
    globalUniformObject.viewProjection = renderingState->sceneCamera.viewProjection;
    globalUniformObject.directionalLight = vec3_create(1, 0, 0);
    UpdateGlobalUniform(&globalUniformObject);

    // ================== Start rendering
    if (!BeginRendering())
        return;

    // ================== Rendering main scene to screen
    RenderTargetStartRendering(GetMainRenderTarget());

    // Rendering the marching cubes mesh
    MaterialBind(renderingState->normalRenderingMaterial);
    MCRenderWorld();

    // Rendering text as a demo of the text system
    mat4 bezierModel = mat4_2Dtranslate(vec2_create(0, 4));
    // TextUpdateTransform(gameState->textTest, mat4_mul_mat4(gameState->uiViewProj, bezierModel));
    TextUpdateTransform(renderingState->textTest, mat4_mul_mat4(renderingState->sceneCamera.viewProjection, bezierModel));
    TextRender();

    // Rendering the debug menu's
    for (u32 i = 0; i < renderingState->debugMenuDarray->size; i++)
    {
        DebugUIRenderMenu(renderingState->debugMenuDarray->data[i]);
    }

    RenderTargetStopRendering(GetMainRenderTarget());

    // ================== Ending rendering
    EndRendering();
}

void GameRenderingShutdown()
{
    UnregisterEventListener(EVCODE_WINDOW_RESIZED, OnWindowResize);

    // Creating darray for debug ui's
    DarrayDestroy(renderingState->debugMenuDarray);

    MCDestroyMeshAndDensityMap();

	MaterialDestroy(renderingState->normalRenderingMaterial);
    MaterialDestroy(renderingState->marchingCubesMaterial);

    Free(GetGlobalAllocator(), renderingState);
}

GameCameras GetGameCameras()
{
    GRASSERT_DEBUG(renderingState);

    GameCameras gameCameras = {};
    gameCameras.sceneCamera = &renderingState->sceneCamera;
    gameCameras.uiCamera = &renderingState->uiCamera;

    return gameCameras;
}

void RegisterDebugMenu(DebugMenu* debugMenu)
{
    DebugMenuRefDarrayPushback(renderingState->debugMenuDarray, &debugMenu);
}

void UnregisterDebugMenu(DebugMenu* debugMenu)
{
    for (u32 i = 0; i < renderingState->debugMenuDarray->size; i++)
    {
        if (renderingState->debugMenuDarray->data[i] == debugMenu)
        {
            DarrayPopAt(renderingState->debugMenuDarray, i);
			return;
        }
    }
}
