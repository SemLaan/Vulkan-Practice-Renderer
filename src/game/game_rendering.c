#include "game_rendering.h"

#include "renderer/render_target.h"
#include "renderer/renderer.h"
#include "math/lin_alg.h"
#include "renderer/ui/text_renderer.h"
#include "core/event.h"
#include "core/platform.h"
#include "marching_cubes.h"

#define LIGHTING_SHADER_NAME "lighting"
#define UI_TEXTURE_SHADER_NAME "uitexture"
#define SHADOW_SHADER_NAME "shadow"
#define INSTANCED_SHADOW_SHADER_NAME "instanced shadow"
#define INSTANCED_LIGHTING_SHADER_NAME "instanced lighting"
#define MARCHING_CUBES_SHADER_NAME "marchingCubes"
#define FONT_NAME_ROBOTO "roboto"
#define FONT_NAME_ADORABLE_HANDMADE "adorable"
#define FONT_NAME_NICOLAST "nicolast"
#define DEFAULT_FOV 45.0f
#define DEFAULT_NEAR_PLANE 1.0f
#define DEFAULT_FAR_PLANE 200.0f
#define UI_NEAR_PLANE -1
#define UI_FAR_PLANE 1
#define UI_ORTHO_HEIGHT 10

typedef struct GameRenderingState
{
    // Materials
    Material instancedShadowMaterial;
    Material shadowMaterial;
    Material lightingMaterial;
    Material instancedLightingMaterial;
    Material uiTextureMaterial;
    Material marchingCubesMaterial;

    // Render targets
    RenderTarget shadowMapRenderTarget;

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
        shaderCreateInfo.vertexBufferLayout.perVertexAttributeCount = 3;
        shaderCreateInfo.vertexBufferLayout.perVertexAttributes[0] = VERTEX_ATTRIBUTE_TYPE_VEC3;
        shaderCreateInfo.vertexBufferLayout.perVertexAttributes[1] = VERTEX_ATTRIBUTE_TYPE_VEC3;
        shaderCreateInfo.vertexBufferLayout.perVertexAttributes[2] = VERTEX_ATTRIBUTE_TYPE_VEC2;

        shaderCreateInfo.vertexShaderName = "simple_shader";
        shaderCreateInfo.fragmentShaderName = "simple_shader";
        shaderCreateInfo.renderTargetColor = true;
        shaderCreateInfo.renderTargetDepth = true;
        ShaderCreate(LIGHTING_SHADER_NAME, &shaderCreateInfo);

        shaderCreateInfo.vertexShaderName = "ui_texture";
        shaderCreateInfo.fragmentShaderName = "ui_texture";
        shaderCreateInfo.renderTargetColor = true;
        shaderCreateInfo.renderTargetDepth = false;
        ShaderCreate(UI_TEXTURE_SHADER_NAME, &shaderCreateInfo);

        shaderCreateInfo.vertexShaderName = "shadow";
        shaderCreateInfo.fragmentShaderName = nullptr;
        shaderCreateInfo.renderTargetColor = false;
        shaderCreateInfo.renderTargetDepth = true;
        ShaderCreate(SHADOW_SHADER_NAME, &shaderCreateInfo);

        shaderCreateInfo.vertexBufferLayout.perInstanceAttributeCount = 1;
        shaderCreateInfo.vertexBufferLayout.perInstanceAttributes[0] = VERTEX_ATTRIBUTE_TYPE_MAT4;
        shaderCreateInfo.vertexShaderName = "shadow_instanced";

        ShaderCreate(INSTANCED_SHADOW_SHADER_NAME, &shaderCreateInfo);

        shaderCreateInfo.vertexShaderName = "simple_shader_instanced";
        shaderCreateInfo.fragmentShaderName = "simple_shader";
        shaderCreateInfo.renderTargetColor = true;
        shaderCreateInfo.renderTargetDepth = true;

        ShaderCreate(INSTANCED_LIGHTING_SHADER_NAME, &shaderCreateInfo);

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
    }

    // Creating materials
    {
        renderingState->shadowMaterial = MaterialCreate(ShaderGetRef(SHADOW_SHADER_NAME));
        renderingState->instancedShadowMaterial = MaterialCreate(ShaderGetRef(INSTANCED_SHADOW_SHADER_NAME));
        renderingState->instancedLightingMaterial = MaterialCreate(ShaderGetRef(INSTANCED_LIGHTING_SHADER_NAME));
        renderingState->lightingMaterial = MaterialCreate(ShaderGetRef(LIGHTING_SHADER_NAME));
        renderingState->uiTextureMaterial = MaterialCreate(ShaderGetRef(UI_TEXTURE_SHADER_NAME));
        renderingState->marchingCubesMaterial = MaterialCreate(ShaderGetRef(MARCHING_CUBES_SHADER_NAME));
    }

	// Creating the shadow map render target
    renderingState->shadowMapRenderTarget = RenderTargetCreate(4000, 4000, RENDER_TARGET_USAGE_NONE, RENDER_TARGET_USAGE_TEXTURE);

    // Initializing material state
    {
        MaterialUpdateTexture(renderingState->uiTextureMaterial, "tex", GetDepthAsTexture(renderingState->shadowMapRenderTarget), SAMPLER_TYPE_LINEAR_CLAMP_EDGE);
        MaterialUpdateTexture(renderingState->lightingMaterial, "shadowMap", GetDepthAsTexture(renderingState->shadowMapRenderTarget), SAMPLER_TYPE_NEAREST_CLAMP_EDGE);
        MaterialUpdateTexture(renderingState->lightingMaterial, "shadowMapCompare", GetDepthAsTexture(renderingState->shadowMapRenderTarget), SAMPLER_TYPE_SHADOW);
        MaterialUpdateTexture(renderingState->instancedLightingMaterial, "shadowMap", GetDepthAsTexture(renderingState->shadowMapRenderTarget), SAMPLER_TYPE_NEAREST_CLAMP_EDGE);
        MaterialUpdateTexture(renderingState->instancedLightingMaterial, "shadowMapCompare", GetDepthAsTexture(renderingState->shadowMapRenderTarget), SAMPLER_TYPE_SHADOW);
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
	// ================== Camera calculations
	CameraRecalculateViewAndViewProjection(&renderingState->sceneCamera);

	GlobalUniformObject globalUniformObject = {};
    globalUniformObject.viewPosition = renderingState->sceneCamera.position;
    globalUniformObject.viewProjection = renderingState->sceneCamera.viewProjection;
    globalUniformObject.directionalLight = vec3_create(1, 0, 0);
    UpdateGlobalUniform(&globalUniformObject);

	// ================== Start rendering
	if (!BeginRendering())
        return;

	// ================== Rendering main scene to screen
    RenderTargetStartRendering(GetMainRenderTarget());

	// Rendering the marching cubes mesh
	MaterialBind(renderingState->marchingCubesMaterial);
    MCRenderWorld();

	// Rendering text as a demo of the text system
    mat4 bezierModel = mat4_2Dtranslate(vec2_create(0, 4));
    // TextUpdateTransform(gameState->textTest, mat4_mul_mat4(gameState->uiViewProj, bezierModel));
    TextUpdateTransform(renderingState->textTest, mat4_mul_mat4(renderingState->sceneCamera.viewProjection, bezierModel));
    TextRender();

	// TODO: fix
	// Rendering the debug menu's
	//DebugUIRenderMenu(gameState->debugMenu);
    //if (gameState->debugMenu2)
    //    DebugUIRenderMenu(gameState->debugMenu2);

	RenderTargetStopRendering(GetMainRenderTarget());

	// ================== Ending rendering
    EndRendering();
}

void GameRenderingShutdown()
{
	UnregisterEventListener(EVCODE_WINDOW_RESIZED, OnWindowResize);

    MCDestroyMeshAndDensityMap();

    RenderTargetDestroy(renderingState->shadowMapRenderTarget);

    MaterialDestroy(renderingState->instancedLightingMaterial);
    MaterialDestroy(renderingState->instancedShadowMaterial);
    MaterialDestroy(renderingState->shadowMaterial);
    MaterialDestroy(renderingState->uiTextureMaterial);
    MaterialDestroy(renderingState->lightingMaterial);

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

