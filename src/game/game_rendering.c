#include "game_rendering.h"

#include "core/event.h"
#include "core/platform.h"
#include "marching_cubes.h"
#include "math/lin_alg.h"
#include "renderer/render_target.h"
#include "renderer/renderer.h"
#include "renderer/ui/text_renderer.h"
#include "renderer/ui/profiling_ui.h"
#include "terrain_density_functions.h"
#include "core/engine.h"
#include "renderer/mesh_optimizer.h"

#define MARCHING_CUBES_SHADER_NAME "marchingCubes"
#define NORMAL_SHADER_NAME "normal_shader"
#define OUTLINE_SHADER_NAME "outline_shader"
#define UI_TEXTURE_NAME "ui_texture_shader"
#define FONT_NAME_ROBOTO "roboto"
#define FONT_NAME_ADORABLE_HANDMADE "adorable"
#define FONT_NAME_NICOLAST "nicolast"
#define DEFAULT_FOV 45.0f
#define DEFAULT_NEAR_PLANE 1.0f
#define DEFAULT_FAR_PLANE 200.0f
#define UI_NEAR_PLANE -1
#define UI_FAR_PLANE 1
#define UI_ORTHO_HEIGHT 10
#define DENSITY_MAP_SIZE 100 // width, heigth and depth of the density map

DEFINE_DARRAY_TYPE_REF(DebugMenu);

typedef struct ShaderParameters
{
    f32 normalEdgeThreshold;
	f32 glyphThresholdSize;
	vec4 uiColor;
	vec4 uiOther;
    bool renderMarchingCubesMesh;
	bool renderMarchingCubesNormals;
	bool renderOutlines;
} ShaderParameters;

typedef struct WorldGenParameters
{
	BezierDensityFuncSettings bezierDensityFuncSettings;

	i64 blurIterations;
	i64 blurKernelSize;
	i64 blurKernelSizeOptions[POSSIBLE_BLUR_KERNEL_SIZES_COUNT];
} WorldGenParameters;

typedef struct World
{
    f32* terrainDensityMap;
    u32 densityMapWidth;
    u32 densityMapHeight;
    u32 densityMapDepth;
    u32 terrainSeed;
    GPUMesh marchingCubesGpuMesh;
} World;

typedef struct GameRenderingState
{
    // Debug menu's
    DebugMenuRefDarray* debugMenuDarray;
    // Debug menu for adjusting shader parameters
    DebugMenu* shaderParamDebugMenu;
	DebugMenu* worldGenParamDebugMenu;

    // Materials
    Material marchingCubesMaterial;
    Material normalRenderingMaterial;
    Material outlineMaterial;
	Material uiTextureMaterial;

    // Render targets
    RenderTarget normalAndDepthRenderTarget;

    // Camera matrices and positions
    Camera sceneCamera;
    Camera uiCamera;

    // World data (meshes and related data)
    World world;

    // Data controlled by debug menu's
    ShaderParameters shaderParameters;
	WorldGenParameters worldGenParams;
} GameRenderingState;

static GameRenderingState* renderingState = nullptr;

// TODO: remove this
static Font* tempFontRef = nullptr;

static bool OnWindowResize(EventCode type, EventData data)
{
    // Recalculating projection matrices
    vec2i windowSize = GetPlatformWindowSize();
    float windowAspectRatio = windowSize.x / (float)windowSize.y;
    renderingState->sceneCamera.projection = mat4_perspective(DEFAULT_FOV, windowAspectRatio, DEFAULT_NEAR_PLANE, DEFAULT_FAR_PLANE);
    renderingState->uiCamera.projection = mat4_orthographic(0, UI_ORTHO_HEIGHT * windowAspectRatio, 0, UI_ORTHO_HEIGHT, UI_NEAR_PLANE, UI_FAR_PLANE);

    // Resizing framebuffer
    RenderTargetDestroy(renderingState->normalAndDepthRenderTarget);
    renderingState->normalAndDepthRenderTarget = RenderTargetCreate(windowSize.x, windowSize.y, RENDER_TARGET_USAGE_TEXTURE, RENDER_TARGET_USAGE_TEXTURE);

    MaterialUpdateTexture(renderingState->outlineMaterial, "depthTex", GetDepthAsTexture(renderingState->normalAndDepthRenderTarget), SAMPLER_TYPE_NEAREST_CLAMP_EDGE);
    MaterialUpdateTexture(renderingState->outlineMaterial, "normalTex", GetColorAsTexture(renderingState->normalAndDepthRenderTarget), SAMPLER_TYPE_NEAREST_CLAMP_EDGE);

    return false;
}

void GameRenderingInit()
{
    renderingState = Alloc(GetGlobalAllocator(), sizeof(*renderingState));

    // Calculating camera projections and listening to the window resize event to recalculate projection on window resize
    vec2i windowSize = GetPlatformWindowSize();
    float windowAspectRatio = windowSize.x / (float)windowSize.y;
    renderingState->sceneCamera.projection = mat4_perspective(DEFAULT_FOV, windowAspectRatio, DEFAULT_NEAR_PLANE, DEFAULT_FAR_PLANE);
    renderingState->uiCamera.projection = mat4_orthographic(0, UI_ORTHO_HEIGHT * windowAspectRatio, 0, UI_ORTHO_HEIGHT, UI_NEAR_PLANE, UI_FAR_PLANE);

    RegisterEventListener(EVCODE_SWAPCHAIN_RESIZED, OnWindowResize);

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

	tempFontRef = TextGetFont(FONT_NAME_ROBOTO);

    // Creating render targets
    {
        renderingState->normalAndDepthRenderTarget = RenderTargetCreate(windowSize.x, windowSize.y, RENDER_TARGET_USAGE_TEXTURE, RENDER_TARGET_USAGE_TEXTURE);
    }

    // Loading shaders
    {
        {
            ShaderCreateInfo shaderCreateInfo = {};
            shaderCreateInfo.renderTargetColor = true;
            shaderCreateInfo.renderTargetDepth = true;
            shaderCreateInfo.renderTargetStencil = false;
            shaderCreateInfo.vertexBufferLayout.perVertexAttributeCount = 2;
            shaderCreateInfo.vertexBufferLayout.perVertexAttributes[0] = VERTEX_ATTRIBUTE_TYPE_VEC3;
            shaderCreateInfo.vertexBufferLayout.perVertexAttributes[1] = VERTEX_ATTRIBUTE_TYPE_VEC3;
            shaderCreateInfo.vertexBufferLayout.perInstanceAttributeCount = 0;

            shaderCreateInfo.vertexShaderName = "marchingCubes";
            shaderCreateInfo.fragmentShaderName = "marchingCubes";
            ShaderCreate(MARCHING_CUBES_SHADER_NAME, &shaderCreateInfo);
        }

        {
            ShaderCreateInfo shaderCreateInfo = {};
            shaderCreateInfo.renderTargetColor = true;
            shaderCreateInfo.renderTargetDepth = true;
            shaderCreateInfo.renderTargetStencil = false;
            shaderCreateInfo.vertexBufferLayout.perVertexAttributeCount = 2;
            shaderCreateInfo.vertexBufferLayout.perVertexAttributes[0] = VERTEX_ATTRIBUTE_TYPE_VEC3;
            shaderCreateInfo.vertexBufferLayout.perVertexAttributes[1] = VERTEX_ATTRIBUTE_TYPE_VEC3;
            shaderCreateInfo.vertexBufferLayout.perInstanceAttributeCount = 0;

            shaderCreateInfo.vertexShaderName = "normal";
            shaderCreateInfo.fragmentShaderName = "normal";
            ShaderCreate(NORMAL_SHADER_NAME, &shaderCreateInfo);
        }

        {
            ShaderCreateInfo shaderCreateInfo = {};
            shaderCreateInfo.renderTargetColor = true;
            shaderCreateInfo.renderTargetDepth = false;
            shaderCreateInfo.renderTargetStencil = false;
            shaderCreateInfo.vertexBufferLayout.perVertexAttributeCount = 2;
            shaderCreateInfo.vertexBufferLayout.perVertexAttributes[0] = VERTEX_ATTRIBUTE_TYPE_VEC3;
            shaderCreateInfo.vertexBufferLayout.perVertexAttributes[1] = VERTEX_ATTRIBUTE_TYPE_VEC2;
            shaderCreateInfo.vertexBufferLayout.perInstanceAttributeCount = 0;

            shaderCreateInfo.vertexShaderName = "fullscreen";
            shaderCreateInfo.fragmentShaderName = "outline";
            ShaderCreate(OUTLINE_SHADER_NAME, &shaderCreateInfo);
        }

		{
            ShaderCreateInfo shaderCreateInfo = {};
            shaderCreateInfo.renderTargetColor = true;
            shaderCreateInfo.renderTargetDepth = false;
            shaderCreateInfo.renderTargetStencil = false;
            shaderCreateInfo.vertexBufferLayout.perVertexAttributeCount = 3;
            shaderCreateInfo.vertexBufferLayout.perVertexAttributes[0] = VERTEX_ATTRIBUTE_TYPE_VEC3;
            shaderCreateInfo.vertexBufferLayout.perVertexAttributes[1] = VERTEX_ATTRIBUTE_TYPE_VEC3;
            shaderCreateInfo.vertexBufferLayout.perVertexAttributes[2] = VERTEX_ATTRIBUTE_TYPE_VEC2;
            shaderCreateInfo.vertexBufferLayout.perInstanceAttributeCount = 0;

            shaderCreateInfo.vertexShaderName = "ui_texture";
            shaderCreateInfo.fragmentShaderName = "ui_texture";
            ShaderCreate(UI_TEXTURE_NAME, &shaderCreateInfo);
        }
    }

    // Creating materials
    {
        renderingState->marchingCubesMaterial = MaterialCreate(ShaderGetRef(MARCHING_CUBES_SHADER_NAME));
        renderingState->normalRenderingMaterial = MaterialCreate(ShaderGetRef(NORMAL_SHADER_NAME));
        renderingState->outlineMaterial = MaterialCreate(ShaderGetRef(OUTLINE_SHADER_NAME));
		renderingState->uiTextureMaterial = MaterialCreate(ShaderGetRef(UI_TEXTURE_NAME));
    }

    // Initializing material state
    {
        vec4 testColor = vec4_create(0.2, 0.4f, 1, 1);
        f32 roughness = 0;
        MaterialUpdateProperty(renderingState->marchingCubesMaterial, "color", &testColor);
        MaterialUpdateProperty(renderingState->marchingCubesMaterial, "roughness", &roughness);
        MaterialUpdateTexture(renderingState->outlineMaterial, "depthTex", GetDepthAsTexture(renderingState->normalAndDepthRenderTarget), SAMPLER_TYPE_NEAREST_CLAMP_EDGE);
        MaterialUpdateTexture(renderingState->outlineMaterial, "normalTex", GetColorAsTexture(renderingState->normalAndDepthRenderTarget), SAMPLER_TYPE_NEAREST_CLAMP_EDGE);
		MaterialUpdateTexture(renderingState->uiTextureMaterial, "tex", tempFontRef->glyphTextureAtlas, SAMPLER_TYPE_LINEAR_CLAMP_EDGE);
    }

    // Setting up debug ui's for shader parameters and terrain generation settings
	renderingState->shaderParameters.renderOutlines = true;
    renderingState->shaderParamDebugMenu = DebugUICreateMenu("Shader Parameters");
    RegisterDebugMenu(renderingState->shaderParamDebugMenu);
    DebugUIAddSliderFloat(renderingState->shaderParamDebugMenu, "edge detection normal threshold", 0.001f, 1, &renderingState->shaderParameters.normalEdgeThreshold);
    DebugUIAddToggleButton(renderingState->shaderParamDebugMenu, "Render marching cubes mesh", &renderingState->shaderParameters.renderMarchingCubesMesh);
    DebugUIAddToggleButton(renderingState->shaderParamDebugMenu, "Render mesh normals", &renderingState->shaderParameters.renderMarchingCubesNormals);
    DebugUIAddToggleButton(renderingState->shaderParamDebugMenu, "Render marching cubes outline", &renderingState->shaderParameters.renderOutlines);
	DebugUIAddSliderFloat(renderingState->shaderParamDebugMenu, "r", 0, 1, &renderingState->shaderParameters.uiColor.x);
    DebugUIAddSliderFloat(renderingState->shaderParamDebugMenu, "g", 0, 1, &renderingState->shaderParameters.uiColor.y);
    DebugUIAddSliderFloat(renderingState->shaderParamDebugMenu, "b", 0, 1, &renderingState->shaderParameters.uiColor.z);
    DebugUIAddSliderLog(renderingState->shaderParamDebugMenu, "edge thickness", 10, 0.01f, 1, &renderingState->shaderParameters.uiOther.x);
    DebugUIAddSliderLog(renderingState->shaderParamDebugMenu, "roundedness", 10, 0.01f, 1, &renderingState->shaderParameters.uiOther.y);
    DebugUIAddSliderLog(renderingState->shaderParamDebugMenu, "transparency transition", 10, 0.01f, 1, &renderingState->shaderParameters.uiOther.z);
	DebugUIAddSliderLog(renderingState->shaderParamDebugMenu, "Glyph Threshold Size", 10, 0.001f, 1.0f, &renderingState->shaderParameters.glyphThresholdSize);
	renderingState->shaderParameters.uiColor.w = 1;

	i64 blurKernelSizeOptions[POSSIBLE_BLUR_KERNEL_SIZES_COUNT] = POSSIBLE_BLUR_KERNEL_SIZES;
	MemoryCopy(renderingState->worldGenParams.blurKernelSizeOptions, blurKernelSizeOptions, sizeof(blurKernelSizeOptions));

	renderingState->worldGenParamDebugMenu = DebugUICreateMenu("World Gen Parameters");
	RegisterDebugMenu(renderingState->worldGenParamDebugMenu);
	DebugUIAddSliderInt(renderingState->worldGenParamDebugMenu, "Blur Iterations", MIN_BLUR_ITERATIONS, MAX_BLUR_ITERATIONS, &renderingState->worldGenParams.blurIterations);
	DebugUIAddSliderDiscrete(renderingState->worldGenParamDebugMenu, "Blur Kernel Size", renderingState->worldGenParams.blurKernelSizeOptions, POSSIBLE_BLUR_KERNEL_SIZES_COUNT, &renderingState->worldGenParams.blurKernelSize);
	DebugUIAddSliderInt(renderingState->worldGenParamDebugMenu, "Bezier tunnel count", MIN_BEZIER_TUNNEL_COUNT, MAX_BEZIER_TUNNEL_COUNT, &renderingState->worldGenParams.bezierDensityFuncSettings.bezierTunnelCount);
	DebugUIAddSliderFloat(renderingState->worldGenParamDebugMenu, "Bezier tunnel radius", MIN_BEZIER_TUNNEL_RADIUS, MAX_BEZIER_TUNNEL_RADIUS, &renderingState->worldGenParams.bezierDensityFuncSettings.bezierTunnelRadius);
	DebugUIAddSliderInt(renderingState->worldGenParamDebugMenu, "Bezier tunnel control points", MIN_BEZIER_TUNNEL_CONTROL_POINTS, MAX_BEZIER_TUNNEL_CONTROL_POINTS, &renderingState->worldGenParams.bezierDensityFuncSettings.bezierTunnelControlPoints);
	DebugUIAddSliderInt(renderingState->worldGenParamDebugMenu, "Sphere hole count", MIN_SPHERE_HOLE_COUNT, MAX_SPHERE_HOLE_COUNT, &renderingState->worldGenParams.bezierDensityFuncSettings.sphereHoleCount);
	DebugUIAddSliderFloat(renderingState->worldGenParamDebugMenu, "Sphere hole radius", MIN_SPHERE_HOLE_RADIUS, MAX_SPHERE_HOLE_RADIUS, &renderingState->worldGenParams.bezierDensityFuncSettings.sphereHoleRadius);

	// Generating marching cubes terrain
    {
		World* world = &renderingState->world;
        world->terrainSeed = 0;
        world->densityMapWidth = DENSITY_MAP_SIZE;
        world->densityMapHeight = DENSITY_MAP_SIZE;
        world->densityMapDepth = DENSITY_MAP_SIZE;
        u32 densityMapValueCount = world->densityMapWidth * world->densityMapHeight * world->densityMapDepth;
        world->terrainDensityMap = Alloc(GetGlobalAllocator(), sizeof(*world->terrainDensityMap) * densityMapValueCount);
        DensityFuncBezierCurveHole(&world->terrainSeed, &renderingState->worldGenParams.bezierDensityFuncSettings, world->terrainDensityMap, world->densityMapWidth, world->densityMapHeight, world->densityMapDepth);
		BlurDensityMapGaussian(renderingState->worldGenParams.blurIterations, renderingState->worldGenParams.blurKernelSize, world->terrainDensityMap, world->densityMapWidth, world->densityMapHeight, world->densityMapDepth);
		MeshData mcMeshData = MarchingCubesGenerateMesh(world->terrainDensityMap, world->densityMapWidth, world->densityMapHeight, world->densityMapDepth);
		world->marchingCubesGpuMesh.vertexBuffer = VertexBufferCreate(mcMeshData.vertices, mcMeshData.vertexStride * mcMeshData.vertexCount);
		world->marchingCubesGpuMesh.indexBuffer = IndexBufferCreate(mcMeshData.indices, mcMeshData.indexCount);
		MarchingCubesFreeMeshData(mcMeshData);
    }
}

void GameRenderingRender()
{
	// ================== Start rendering
    if (!BeginRendering())
        return;

    vec4 testColor = vec4_create(0.2, 0.4f, 1, 1);
    f32 roughness = 0;
    MaterialUpdateProperty(renderingState->marchingCubesMaterial, "color", &testColor);
    MaterialUpdateProperty(renderingState->marchingCubesMaterial, "roughness", &roughness);
	DebugUISetMaterialValues(renderingState->shaderParamDebugMenu, renderingState->shaderParameters.uiColor, renderingState->shaderParameters.uiOther);
    f32 nearPlane = DEFAULT_NEAR_PLANE;
    f32 farPlane = DEFAULT_FAR_PLANE;
    vec2i windowSize = GetPlatformWindowSize();
    MaterialUpdateProperty(renderingState->outlineMaterial, "zNear", &nearPlane);
    MaterialUpdateProperty(renderingState->outlineMaterial, "zFar", &farPlane);
    MaterialUpdateProperty(renderingState->outlineMaterial, "screenWidth", &windowSize.x);
    MaterialUpdateProperty(renderingState->outlineMaterial, "screenHeight", &windowSize.y);
    MaterialUpdateProperty(renderingState->outlineMaterial, "normalEdgeThreshold", &renderingState->shaderParameters.normalEdgeThreshold);
	MaterialUpdateProperty(renderingState->uiTextureMaterial, "uiProjection", &renderingState->uiCamera.projection);
	mat4 identity = mat4_identity();
	MaterialUpdateProperty(renderingState->uiTextureMaterial, "glyphThresholdSize", &renderingState->shaderParameters.glyphThresholdSize);

    // ================== Camera calculations
    CameraRecalculateViewAndViewProjection(&renderingState->sceneCamera);

    GlobalUniformObject globalUniformObject = {};
    globalUniformObject.viewPosition = vec3_invert_sign(renderingState->sceneCamera.position);
    globalUniformObject.viewProjection = renderingState->sceneCamera.viewProjection;
    globalUniformObject.directionalLight = vec3_create(1, 0, 0);
    UpdateGlobalUniform(&globalUniformObject);

    // ================== Rendering normals and depth of the marching cubes mesh
    RenderTargetStartRendering(renderingState->normalAndDepthRenderTarget);

    // Rendering the marching cubes mesh
    MaterialBind(renderingState->normalRenderingMaterial);
	Draw(1, &renderingState->world.marchingCubesGpuMesh.vertexBuffer, renderingState->world.marchingCubesGpuMesh.indexBuffer, &identity, 1);

    RenderTargetStopRendering(renderingState->normalAndDepthRenderTarget);

    // ================== Rendering main scene to screen
    RenderTargetStartRendering(GetMainRenderTarget());

    if (renderingState->shaderParameters.renderMarchingCubesMesh)
    {
		if (renderingState->shaderParameters.renderMarchingCubesNormals)
			MaterialBind(renderingState->normalRenderingMaterial);
		else
	        MaterialBind(renderingState->marchingCubesMaterial);
		Draw(1, &renderingState->world.marchingCubesGpuMesh.vertexBuffer, renderingState->world.marchingCubesGpuMesh.indexBuffer, &identity, 1);
    }

    // Rendering the marching cubes mesh outline
	if (renderingState->shaderParameters.renderOutlines)
	{
		MaterialBind(renderingState->outlineMaterial);
		GPUMesh* fullscreenTriangleMesh = GetBasicMesh(BASIC_MESH_NAME_FULL_SCREEN_TRIANGLE);
		Draw(1, &fullscreenTriangleMesh->vertexBuffer, fullscreenTriangleMesh->indexBuffer, nullptr, 1);
	}

    // Rendering the debug menu's
    DebugUIRenderMenus();

	// TODO: this renders the glyph texture atlas, once texture rendering has been added to the debug ui this needs to be removed and rendered with the debug ui
	//MeshData* quadMesh = GetBasicMesh(BASIC_MESH_NAME_QUAD);
	//mat4 quadModelMatrix = mat4_mul_mat4(mat4_2Dtranslate(vec2_create(4, 4)), mat4_2Dscale(vec2_create(3, 3)));
	//MaterialBind(renderingState->uiTextureMaterial);
	//Draw(1, &quadMesh->vertexBuffer, quadMesh->indexBuffer, &quadModelMatrix, 1);

	DrawFrameStats();

    RenderTargetStopRendering(GetMainRenderTarget());

    // ================== Ending rendering
    EndRendering();
}

void GameRenderingShutdown()
{
    UnregisterEventListener(EVCODE_SWAPCHAIN_RESIZED, OnWindowResize);

	TextUnloadFont(FONT_NAME_ROBOTO);
    TextUnloadFont(FONT_NAME_ADORABLE_HANDMADE);
    TextUnloadFont(FONT_NAME_NICOLAST);

    // Destroying debug menu for shader params and darray for debug ui's
    UnregisterDebugMenu(renderingState->shaderParamDebugMenu);
    DebugUIDestroyMenu(renderingState->shaderParamDebugMenu);
    DarrayDestroy(renderingState->debugMenuDarray);

	// Destroying debug menu for world gen parameters
	UnregisterDebugMenu(renderingState->worldGenParamDebugMenu);
    DebugUIDestroyMenu(renderingState->worldGenParamDebugMenu);

    // Destroying world data
	{
		World* world = &renderingState->world;
		Free(GetGlobalAllocator(), world->terrainDensityMap);
		VertexBufferDestroy(world->marchingCubesGpuMesh.vertexBuffer);
		IndexBufferDestroy(world->marchingCubesGpuMesh.indexBuffer);
	}

    MaterialDestroy(renderingState->outlineMaterial);
    MaterialDestroy(renderingState->normalRenderingMaterial);
    MaterialDestroy(renderingState->marchingCubesMaterial);
    MaterialDestroy(renderingState->uiTextureMaterial);

    RenderTargetDestroy(renderingState->normalAndDepthRenderTarget);

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

void RegenerateMarchingCubesMesh()
{
	World* world = &renderingState->world;
	DensityFuncBezierCurveHole(&world->terrainSeed, &renderingState->worldGenParams.bezierDensityFuncSettings, world->terrainDensityMap, world->densityMapWidth, world->densityMapHeight, world->densityMapDepth);
	BlurDensityMapGaussian(renderingState->worldGenParams.blurIterations, renderingState->worldGenParams.blurKernelSize, world->terrainDensityMap, world->densityMapWidth, world->densityMapHeight, world->densityMapDepth);

	VertexBufferDestroy(world->marchingCubesGpuMesh.vertexBuffer);
	IndexBufferDestroy(world->marchingCubesGpuMesh.indexBuffer);

	MeshData mcMeshData = MarchingCubesGenerateMesh(world->terrainDensityMap, world->densityMapWidth, world->densityMapHeight, world->densityMapDepth);

	MeshData smoothedMCMeshData = MeshOptimizerMergeNormals(mcMeshData, offsetof(VertexT2, position), offsetof(VertexT2, normal));

	world->marchingCubesGpuMesh.vertexBuffer = VertexBufferCreate(smoothedMCMeshData.vertices, smoothedMCMeshData.vertexStride * smoothedMCMeshData.vertexCount);
	world->marchingCubesGpuMesh.indexBuffer = IndexBufferCreate(smoothedMCMeshData.indices, smoothedMCMeshData.indexCount);

	MarchingCubesFreeMeshData(mcMeshData);
	MeshOptimizerFreeMeshData(smoothedMCMeshData);
}
