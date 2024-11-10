#include "game.h"

#include "containers/darray.h"
#include "core/event.h"
#include "core/input.h"
#include "core/logger.h"
#include "core/platform.h"
#include "core/timer.h"
#include "math/lin_alg.h"
#include "renderer/material.h"
#include "renderer/obj_loader.h"
#include "renderer/render_target.h"
#include "renderer/renderer.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include "renderer/ui/text_renderer.h"
#include "renderer/ui/debug_ui.h"

#define LIGHTING_SHADER_NAME "lighting"
#define UI_TEXTURE_SHADER_NAME "uitexture"
#define SHADOW_SHADER_NAME "shadow"
#define INSTANCED_SHADOW_SHADER_NAME "instanced shadow"
#define INSTANCED_LIGHTING_SHADER_NAME "instanced lighting"
#define FONT_NAME_ROBOTO "roboto"
#define FONT_NAME_ADORABLE_HANDMADE "adorable"
#define FONT_NAME_NICOLAST "nicolast"

typedef struct Scene
{
    VertexBuffer sphereVB;
    IndexBuffer sphereIB;
    VertexBuffer instancedVB;
    VertexBuffer* vertexBufferDarray;
    IndexBuffer* indexBufferDarray;
    mat4* modelMatrixDarray;
    u32 instanceCount;
} Scene;

typedef struct GameState
{
    Scene scene;
    Timer timer;
    RenderTarget shadowMapRenderTarget;
    Material instancedShadowMaterial;
    Material shadowMaterial;
    Material lightingMaterial;
    Material instancedLightingMaterial;
    Material uiTextureMaterial;
    Text* textTest;
	DebugMenu* debugMenu;
	DebugMenu* debugMenu2;
    Texture texture;
    vec3 camPosition;
    vec3 camRotation;
    mat4 uiViewProj;
    mat4 view;
    mat4 proj;
	f32 mouseMoveSpeed;
    bool mouseEnabled;
	bool mouseEnableButtonPressed;
    bool perspectiveEnabled;
	bool destroyDebugMenu2;
} GameState;

GameState* gameState = nullptr;

#define QUAD_VERT_COUNT 4
#define QUAD_INDEX_COUNT 6

static bool OnWindowResize(EventCode type, EventData data)
{
    vec2i windowSize = GetPlatformWindowSize();
    float windowAspectRatio = windowSize.x / (float)windowSize.y;
    gameState->proj = mat4_perspective(45.0f, windowAspectRatio, 1.0f, 200.0f);
    gameState->uiViewProj = mat4_orthographic(0, 10 * windowAspectRatio, 0, 10, -1, 1);

    return false;
}

void GameInit()
{
    TextLoadFont(FONT_NAME_ROBOTO, "Roboto-Black.ttf");
    TextLoadFont(FONT_NAME_ADORABLE_HANDMADE, "Adorable Handmade.ttf");
    TextLoadFont(FONT_NAME_NICOLAST, "Nicolast.ttf");

    gameState = Alloc(GetGlobalAllocator(), sizeof(*gameState), MEM_TAG_GAME);

    // Creating the scene
    {
        Scene* scene = &gameState->scene;
        scene->vertexBufferDarray = DarrayCreate(sizeof(VertexBuffer), 5, GetGlobalAllocator(), MEM_TAG_GAME);
        scene->indexBufferDarray = DarrayCreate(sizeof(IndexBuffer), 5, GetGlobalAllocator(), MEM_TAG_GAME);
        scene->modelMatrixDarray = DarrayCreate(sizeof(mat4), 5, GetGlobalAllocator(), MEM_TAG_GAME);

        VertexBuffer vb;
        IndexBuffer ib;
        mat4 modelMatrix;
        MeshData* tempMesh;

        // Loading gun
        modelMatrix = mat4_3Dtranslate(vec3_create(0, 3, 0));
        LoadObj("models/beefy_gun.obj", &vb, &ib, false);

        scene->vertexBufferDarray = DarrayPushback(scene->vertexBufferDarray, &vb);
        scene->indexBufferDarray = DarrayPushback(scene->indexBufferDarray, &ib);
        scene->modelMatrixDarray = DarrayPushback(scene->modelMatrixDarray, &modelMatrix);

        // Loading quad
        modelMatrix = mat4_3Dscale(vec3_create(20, 1, 20));
        tempMesh = GetBasicMesh(BASIC_MESH_NAME_QUAD);

        scene->vertexBufferDarray = DarrayPushback(scene->vertexBufferDarray, &tempMesh->vertexBuffer);
        scene->indexBufferDarray = DarrayPushback(scene->indexBufferDarray, &tempMesh->indexBuffer);
        scene->modelMatrixDarray = DarrayPushback(scene->modelMatrixDarray, &modelMatrix);

        // Loading sphere
        modelMatrix = mat4_3Dtranslate(vec3_create(0, 10, 10));
        LoadObj("models/sphere.obj", &scene->sphereVB, &scene->sphereIB, false);

        // scene->vertexBufferDarray = DarrayPushback(scene->vertexBufferDarray, &vb);
        // scene->indexBufferDarray = DarrayPushback(scene->indexBufferDarray, &ib);
        // scene->modelMatrixDarray = DarrayPushback(scene->modelMatrixDarray, &modelMatrix);

        // Loading cube
        modelMatrix = mat4_3Dtranslate(vec3_create(10, 1, -5));
        tempMesh = GetBasicMesh(BASIC_MESH_NAME_CUBE);

        scene->vertexBufferDarray = DarrayPushback(scene->vertexBufferDarray, &tempMesh->vertexBuffer);
        scene->indexBufferDarray = DarrayPushback(scene->indexBufferDarray, &tempMesh->indexBuffer);
        scene->modelMatrixDarray = DarrayPushback(scene->modelMatrixDarray, &modelMatrix);
    }

    // Initializing rendering state
    gameState->shadowMapRenderTarget = RenderTargetCreate(4000, 4000, RENDER_TARGET_USAGE_NONE, RENDER_TARGET_USAGE_TEXTURE);

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

    gameState->shadowMaterial = MaterialCreate(ShaderGetRef(SHADOW_SHADER_NAME));
    gameState->instancedShadowMaterial = MaterialCreate(ShaderGetRef(INSTANCED_SHADOW_SHADER_NAME));
    gameState->instancedLightingMaterial = MaterialCreate(ShaderGetRef(INSTANCED_LIGHTING_SHADER_NAME));
    gameState->lightingMaterial = MaterialCreate(ShaderGetRef(LIGHTING_SHADER_NAME));
    gameState->uiTextureMaterial = MaterialCreate(ShaderGetRef(UI_TEXTURE_SHADER_NAME));
    MaterialUpdateTexture(gameState->uiTextureMaterial, "tex", GetDepthAsTexture(gameState->shadowMapRenderTarget), SAMPLER_TYPE_LINEAR_CLAMP_EDGE);
    MaterialUpdateTexture(gameState->lightingMaterial, "shadowMap", GetDepthAsTexture(gameState->shadowMapRenderTarget), SAMPLER_TYPE_NEAREST_CLAMP_EDGE);
    MaterialUpdateTexture(gameState->lightingMaterial, "shadowMapCompare", GetDepthAsTexture(gameState->shadowMapRenderTarget), SAMPLER_TYPE_SHADOW);
    MaterialUpdateTexture(gameState->instancedLightingMaterial, "shadowMap", GetDepthAsTexture(gameState->shadowMapRenderTarget), SAMPLER_TYPE_NEAREST_CLAMP_EDGE);
    MaterialUpdateTexture(gameState->instancedLightingMaterial, "shadowMapCompare", GetDepthAsTexture(gameState->shadowMapRenderTarget), SAMPLER_TYPE_SHADOW);

    u8 pixels[TEXTURE_CHANNELS * 2];
    pixels[0] = 255;
    pixels[1] = 0;
    pixels[2] = 255;
    pixels[3] = 255;
    pixels[4] = 125;
    pixels[5] = 255;
    pixels[6] = 255;
    pixels[7] = 255;

    gameState->texture = TextureCreate(2, 1, pixels);

    // Initializing camera
    vec2i windowSize = GetPlatformWindowSize();
    float windowAspectRatio = windowSize.x / (float)windowSize.y;
    gameState->proj = mat4_perspective(45.0f, windowAspectRatio, 1.0f, 200.0f);
    gameState->view = mat4_identity();
    gameState->camPosition = (vec3){0, -5, -10};
    gameState->camRotation = (vec3){0, 0, 0};

    gameState->mouseEnabled = false;
	gameState->mouseEnableButtonPressed = false;
    gameState->perspectiveEnabled = true;
	gameState->destroyDebugMenu2 = false;

    // Calculating UI projection
    gameState->uiViewProj = mat4_orthographic(0, 10 * windowAspectRatio, 0, 10, -1, 1);

    // Registering event listener for recalculating projection matrices if the window gets resized.
    RegisterEventListener(EVCODE_WINDOW_RESIZED, OnWindowResize);

    // Creating test text
    const char* testString = "Beefy text testing!?.";

    gameState->textTest = TextCreate(testString, FONT_NAME_ROBOTO, gameState->uiViewProj, UPDATE_FREQUENCY_STATIC);

	// Creating debug menu
	gameState->debugMenu = DebugUICreateMenu();
	DebugUIAddButton(gameState->debugMenu, "test", nullptr, &gameState->mouseEnableButtonPressed);
	DebugUIAddButton(gameState->debugMenu, "test2", nullptr, nullptr);
	DebugUIAddSlider(gameState->debugMenu, "mouse move speed", 1, 10000, &gameState->mouseMoveSpeed);

	// Testing multiple debug menus
	gameState->debugMenu2 = DebugUICreateMenu();
	DebugUIAddButton(gameState->debugMenu2, "test", nullptr, &gameState->mouseEnableButtonPressed);
	DebugUIAddButton(gameState->debugMenu2, "test2", nullptr, &gameState->destroyDebugMenu2);
	DebugUIAddSlider(gameState->debugMenu2, "mouse move speed", 1, 10000, &gameState->mouseMoveSpeed);

    StartOrResetTimer(&gameState->timer);
}

void GameShutdown()
{
	if (gameState->debugMenu2)
		DebugUIDestroyMenu(gameState->debugMenu2);
	DebugUIDestroyMenu(gameState->debugMenu);

    UnregisterEventListener(EVCODE_WINDOW_RESIZED, OnWindowResize);

    MaterialDestroy(gameState->instancedLightingMaterial);
    MaterialDestroy(gameState->instancedShadowMaterial);
    MaterialDestroy(gameState->shadowMaterial);
    MaterialDestroy(gameState->uiTextureMaterial);
    RenderTargetDestroy(gameState->shadowMapRenderTarget);
    MaterialDestroy(gameState->lightingMaterial);
    TextureDestroy(gameState->texture);

    // Destroying the tracker gan mesh because it is the only mesh actually created by this script
    VertexBufferDestroy(gameState->scene.vertexBufferDarray[0]);
    IndexBufferDestroy(gameState->scene.indexBufferDarray[0]);

    VertexBufferDestroy(gameState->scene.sphereVB);
    IndexBufferDestroy(gameState->scene.sphereIB);

    DarrayDestroy(gameState->scene.vertexBufferDarray);
    DarrayDestroy(gameState->scene.indexBufferDarray);
    DarrayDestroy(gameState->scene.modelMatrixDarray);

    Free(GetGlobalAllocator(), gameState);
}

void GameUpdateAndRender()
{
    // =========================== Update ===================================
	if (gameState->destroyDebugMenu2 && gameState->debugMenu2)
	{
		DebugUIDestroyMenu(gameState->debugMenu2);
		gameState->debugMenu2 = nullptr;
	}

    if (gameState->mouseEnabled)
    {
        gameState->camRotation.y -= GetMouseDistanceFromCenter().x / gameState->mouseMoveSpeed;
        gameState->camRotation.x -= GetMouseDistanceFromCenter().y / gameState->mouseMoveSpeed;
    }
    if (gameState->camRotation.x > 1.5f)
        gameState->camRotation.x = 1.5f;
    if (gameState->camRotation.x < -1.5f)
        gameState->camRotation.x = -1.5f;

    // Create the rotation matrix
    mat4 rotation = mat4_rotate_xyz(gameState->camRotation);

    vec3 forwardVector = {-rotation.values[2 + COL4(0)], -rotation.values[2 + COL4(1)], -rotation.values[2 + COL4(2)]};
    vec3 rightVector = {rotation.values[0 + COL4(0)], rotation.values[0 + COL4(1)], rotation.values[0 + COL4(2)]};

    vec3 frameMovement = {};

    if (GetKeyDown(KEY_A))
        frameMovement = vec3_add_vec3(frameMovement, rightVector);
    if (GetKeyDown(KEY_D))
        frameMovement = vec3_sub_vec3(frameMovement, rightVector);
    if (GetKeyDown(KEY_S))
        frameMovement = vec3_add_vec3(frameMovement, forwardVector);
    if (GetKeyDown(KEY_W))
        frameMovement = vec3_sub_vec3(frameMovement, forwardVector);
    if (GetKeyDown(KEY_SHIFT))
        frameMovement.y += 1;
    if (GetKeyDown(KEY_SPACE))
        frameMovement.y -= 1;
    gameState->camPosition = vec3_add_vec3(gameState->camPosition, vec3_div_float(frameMovement, 300.f));

    mat4 translate = mat4_3Dtranslate(gameState->camPosition);

    gameState->view = mat4_mul_mat4(rotation, translate);

	// If the mouse button enable button is pressed or if the mouse is enabled and the player presses it.
    if ((gameState->mouseEnableButtonPressed) ||
		(gameState->mouseEnabled && GetButtonDown(BUTTON_LEFTMOUSEBTN) && !GetButtonDownPrevious(BUTTON_LEFTMOUSEBTN)))
    {
        gameState->mouseEnabled = !gameState->mouseEnabled;
        InputSetMouseCentered(gameState->mouseEnabled);
		gameState->mouseEnableButtonPressed = false;
    }

    // ============================ Rendering ===================================
    mat4 projView = mat4_mul_mat4(gameState->proj, gameState->view);
    vec4 testColor = vec4_create(0.2, 0.4f, 1, 1);
    f32 roughness = 0; // sin(TimerSecondsSinceStart(gameState->timer)) / 2 + 0.5f;
    MaterialUpdateProperty(gameState->lightingMaterial, "color", &testColor);
    MaterialUpdateProperty(gameState->lightingMaterial, "roughness", &roughness);
    MaterialUpdateProperty(gameState->instancedLightingMaterial, "color", &testColor);
    MaterialUpdateProperty(gameState->instancedLightingMaterial, "roughness", &roughness);

    MaterialUpdateProperty(gameState->uiTextureMaterial, "uiProjection", &gameState->uiViewProj);

    // vec3 lightRotationVec = vec3_create(0.5f + sin(TimerSecondsSinceStart(gameState->timer))/2, TimerSecondsSinceStart(gameState->timer), 0);
    vec3 lightRotationVec = vec3_create(0.5f, PI / 2, 0);
    mat4 shadowRotation = mat4_rotate_xyz(vec3_invert_sign(lightRotationVec));

    vec3 directionalLight = {shadowRotation.values[2 + COL4(0)], shadowRotation.values[2 + COL4(1)], shadowRotation.values[2 + COL4(2)]};

    f32 zNear = 1;
    f32 zFar = 200;
    mat4 shadowProj = mat4_orthographic(-20, 20, -20, 20, zNear, zFar);
    mat4 shadowTranslate = mat4_3Dtranslate(vec3_create(0, 0, -40));
    mat4 shadowView = mat4_mul_mat4(shadowTranslate, shadowRotation);
    mat4 shadowProjView = mat4_mul_mat4(shadowProj, shadowView);
    MaterialUpdateProperty(gameState->shadowMaterial, "shadowProjView", &shadowProjView);
    MaterialUpdateProperty(gameState->lightingMaterial, "lightTransform", &shadowProjView);
    MaterialUpdateProperty(gameState->instancedShadowMaterial, "shadowProjView", &shadowProjView);
    MaterialUpdateProperty(gameState->instancedLightingMaterial, "lightTransform", &shadowProjView);
    MaterialUpdateProperty(gameState->uiTextureMaterial, "zNear", &zNear);
    MaterialUpdateProperty(gameState->uiTextureMaterial, "zFar", &zFar);

    GlobalUniformObject globalUniformObject = {};
    globalUniformObject.viewPosition = vec3_invert_sign(gameState->camPosition);
    globalUniformObject.projView = projView;
    globalUniformObject.directionalLight = directionalLight;
    UpdateGlobalUniform(&globalUniformObject);

    // ======================================================= Rendering ================================================
    if (!BeginRendering())
        return;

    // ================== Rendering shadowmap
    RenderTargetStartRendering(gameState->shadowMapRenderTarget);

    MaterialBind(gameState->shadowMaterial);

    for (int i = 0; i < DarrayGetSize(gameState->scene.vertexBufferDarray); i++)
    {
        Draw(1, &gameState->scene.vertexBufferDarray[i], gameState->scene.indexBufferDarray[i], &gameState->scene.modelMatrixDarray[i], 1);
    }

    RenderTargetStopRendering(gameState->shadowMapRenderTarget);

    // ================== Rendering scene
    RenderTargetStartRendering(GetMainRenderTarget());

    MaterialBind(gameState->lightingMaterial);

    for (int i = 0; i < DarrayGetSize(gameState->scene.vertexBufferDarray); i++)
    {
        Draw(1, &gameState->scene.vertexBufferDarray[i], gameState->scene.indexBufferDarray[i], &gameState->scene.modelMatrixDarray[i], 1);
    }

    // Text
    mat4 bezierModel = mat4_2Dtranslate(vec2_create(0, 4));
    // TextUpdateTransform(gameState->textTest, mat4_mul_mat4(gameState->uiViewProj, bezierModel));
    TextUpdateTransform(gameState->textTest, mat4_mul_mat4(projView, bezierModel));
    TextRender();

    MaterialBind(gameState->uiTextureMaterial);

    vec2i windowSize = GetPlatformWindowSize();
    float windowAspectRatio = windowSize.x / (float)windowSize.y;
    mat4 translation = mat4_2Dtranslate((vec2){windowAspectRatio, 1});
    mat4 rotate = mat4_rotate_x(-PI / 2);
    mat4 scale = mat4_2Dscale((vec2){windowAspectRatio, 1});
    mat4 modelMatrix = mat4_mul_mat4(translation, mat4_mul_mat4(rotate, scale));
    Draw(1, &gameState->scene.vertexBufferDarray[1], gameState->scene.indexBufferDarray[1], &modelMatrix, 1);

	DebugUIRenderMenu(gameState->debugMenu);
	if (gameState->debugMenu2)
		DebugUIRenderMenu(gameState->debugMenu2);

    RenderTargetStopRendering(GetMainRenderTarget());

    EndRendering();
}
