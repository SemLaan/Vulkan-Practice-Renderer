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

typedef struct Scene
{
    VertexBuffer* vertexBufferDarray;
    IndexBuffer* indexBufferDarray;
    mat4* modelMatrixDarray;
} Scene;

typedef struct GameState
{
    Scene scene;
    Timer timer;
    RenderTarget shadowMapRenderTarget;
    Shader lightingShader;
    Shader uiTextureShader;
    Shader shadowShader;
    Material shadowMaterial;
    Material lightingMaterial;
    Material uiTextureMaterial;
    Texture texture;
    vec3 camPosition;
    vec3 camRotation;
    mat4 view;
    mat4 proj;
    bool mouseEnabled;
    bool perspectiveEnabled;
} GameState;

GameState* gameState = nullptr;

#define QUAD_VERT_COUNT 4
#define QUAD_INDEX_COUNT 6

bool OnWindowResize(EventCode type, EventData data)
{
    vec2i windowSize = GetPlatformWindowSize();
    float windowAspectRatio = windowSize.x / (float)windowSize.y;
    gameState->proj = mat4_perspective(45.0f, windowAspectRatio, 1.0f, 200.0f);
    return false;
}

void GameInit()
{
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

        // Loading gun
        modelMatrix = mat4_3Dtranslate(vec3_create(0, 3, 0));
        LoadObj("models/beefy_gun.obj", &vb, &ib, false);

        scene->vertexBufferDarray = DarrayPushback(scene->vertexBufferDarray, &vb);
        scene->indexBufferDarray = DarrayPushback(scene->indexBufferDarray, &ib);
        scene->modelMatrixDarray = DarrayPushback(scene->modelMatrixDarray, &modelMatrix);

        // Loading quad
        modelMatrix = mat4_3Dscale(vec3_create(20, 1, 20));
        LoadObj("models/quad.obj", &vb, &ib, false);

        scene->vertexBufferDarray = DarrayPushback(scene->vertexBufferDarray, &vb);
        scene->indexBufferDarray = DarrayPushback(scene->indexBufferDarray, &ib);
        scene->modelMatrixDarray = DarrayPushback(scene->modelMatrixDarray, &modelMatrix);

        // Loading sphere
        modelMatrix = mat4_3Dtranslate(vec3_create(10, 1, 10));
        LoadObj("models/sphere.obj", &vb, &ib, false);

        scene->vertexBufferDarray = DarrayPushback(scene->vertexBufferDarray, &vb);
        scene->indexBufferDarray = DarrayPushback(scene->indexBufferDarray, &ib);
        scene->modelMatrixDarray = DarrayPushback(scene->modelMatrixDarray, &modelMatrix);

        // Loading cube
        modelMatrix = mat4_3Dtranslate(vec3_create(10, 1, -5));
        LoadObj("models/cube.obj", &vb, &ib, false);

        scene->vertexBufferDarray = DarrayPushback(scene->vertexBufferDarray, &vb);
        scene->indexBufferDarray = DarrayPushback(scene->indexBufferDarray, &ib);
        scene->modelMatrixDarray = DarrayPushback(scene->modelMatrixDarray, &modelMatrix);
    }

    // Initializing rendering state
    gameState->shadowMapRenderTarget = RenderTargetCreate(4000, 4000, RENDER_TARGET_USAGE_NONE, RENDER_TARGET_USAGE_TEXTURE);

    ShaderCreateInfo shaderCreateInfo = {};
    shaderCreateInfo.renderTargetStencil = false;

    shaderCreateInfo.vertexShaderName = "simple_shader";
    shaderCreateInfo.fragmentShaderName = "simple_shader";
    shaderCreateInfo.renderTargetColor = true;
    shaderCreateInfo.renderTargetDepth = true;
    gameState->lightingShader = ShaderCreate(&shaderCreateInfo);

    shaderCreateInfo.vertexShaderName = "ui_texture";
    shaderCreateInfo.fragmentShaderName = "ui_texture";
    shaderCreateInfo.renderTargetColor = true;
    shaderCreateInfo.renderTargetDepth = false;
    gameState->uiTextureShader = ShaderCreate(&shaderCreateInfo);

    shaderCreateInfo.vertexShaderName = "shadow";
    shaderCreateInfo.fragmentShaderName = nullptr;
    shaderCreateInfo.renderTargetColor = false;
    shaderCreateInfo.renderTargetDepth = true;
    gameState->shadowShader = ShaderCreate(&shaderCreateInfo);

    gameState->shadowMaterial = MaterialCreate(gameState->shadowShader);
    gameState->lightingMaterial = MaterialCreate(gameState->lightingShader);
    gameState->uiTextureMaterial = MaterialCreate(gameState->uiTextureShader);
    MaterialUpdateTexture(gameState->uiTextureMaterial, "tex", GetDepthAsTexture(gameState->shadowMapRenderTarget), SAMPLER_TYPE_LINEAR_CLAMP_EDGE);
    MaterialUpdateTexture(gameState->lightingMaterial, "shadowMap", GetDepthAsTexture(gameState->shadowMapRenderTarget), SAMPLER_TYPE_LINEAR_CLAMP_EDGE);
    MaterialUpdateTexture(gameState->lightingMaterial, "shadowMapCompare", GetDepthAsTexture(gameState->shadowMapRenderTarget), SAMPLER_TYPE_SHADOW);

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
    gameState->perspectiveEnabled = true;

    RegisterEventListener(EVCODE_WINDOW_RESIZED, OnWindowResize);

    StartOrResetTimer(&gameState->timer);
}

void GameShutdown()
{
    UnregisterEventListener(EVCODE_WINDOW_RESIZED, OnWindowResize);

    MaterialDestroy(gameState->shadowMaterial);
    ShaderDestroy(gameState->shadowShader);
    MaterialDestroy(gameState->uiTextureMaterial);
    ShaderDestroy(gameState->uiTextureShader);
    RenderTargetDestroy(gameState->shadowMapRenderTarget);
    MaterialDestroy(gameState->lightingMaterial);
    ShaderDestroy(gameState->lightingShader);
    TextureDestroy(gameState->texture);

    for (int i = 0; i < DarrayGetSize(gameState->scene.vertexBufferDarray); i++)
    {
        VertexBufferDestroy(gameState->scene.vertexBufferDarray[i]);
        IndexBufferDestroy(gameState->scene.indexBufferDarray[i]);
    }

    DarrayDestroy(gameState->scene.vertexBufferDarray);
    DarrayDestroy(gameState->scene.indexBufferDarray);
    DarrayDestroy(gameState->scene.modelMatrixDarray);

    Free(GetGlobalAllocator(), gameState);
}

void GameUpdateAndRender()
{
    // =========================== Update ===================================
    f32 mouseMoveSpeed = 3500;

    if (gameState->mouseEnabled)
    {
        gameState->camRotation.y -= GetMouseDistanceFromCenter().x / mouseMoveSpeed;
        gameState->camRotation.x -= GetMouseDistanceFromCenter().y / mouseMoveSpeed;
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
        frameMovement = vec3_min_vec3(frameMovement, rightVector);
    if (GetKeyDown(KEY_S))
        frameMovement = vec3_add_vec3(frameMovement, forwardVector);
    if (GetKeyDown(KEY_W))
        frameMovement = vec3_min_vec3(frameMovement, forwardVector);
    if (GetKeyDown(KEY_SHIFT))
        frameMovement.y += 1;
    if (GetKeyDown(KEY_SPACE))
        frameMovement.y -= 1;
    gameState->camPosition = vec3_add_vec3(gameState->camPosition, vec3_div_float(frameMovement, 300.f));

    mat4 translate = mat4_3Dtranslate(gameState->camPosition);

    gameState->view = mat4_mul_mat4(rotation, translate);

    if (GetButtonDown(BUTTON_LEFTMOUSEBTN) && !GetButtonDownPrevious(BUTTON_LEFTMOUSEBTN))
    {
        gameState->mouseEnabled = !gameState->mouseEnabled;
        InputSetMouseCentered(gameState->mouseEnabled);
    }

    // ============================ Rendering ===================================
    mat4 projView = mat4_mul_mat4(gameState->proj, gameState->view);
    vec4 testColor = vec4_create(0.2, 0.4f, 1, 1);
    f32 roughness = 0; // sin(TimerSecondsSinceStart(gameState->timer)) / 2 + 0.5f;
    MaterialUpdateProperty(gameState->lightingMaterial, "color", &testColor);
    MaterialUpdateProperty(gameState->lightingMaterial, "roughness", &roughness);

    

    vec2i windowSize = GetPlatformWindowSize();
    f32 windowAspectRatio = windowSize.x / (float)windowSize.y;
    mat4 uiProj = mat4_orthographic(0, 10 * windowAspectRatio, 0, 10, -1, 1);
    MaterialUpdateProperty(gameState->uiTextureMaterial, "uiProjection", &uiProj);

    //vec3 lightRotationVec = vec3_create(0.5f + sin(TimerSecondsSinceStart(gameState->timer))/2, TimerSecondsSinceStart(gameState->timer), 0);
    vec3 lightRotationVec = vec3_create(0.5f, PI/2, 0);
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
    MaterialUpdateProperty(gameState->uiTextureMaterial, "zNear", &zNear);
    MaterialUpdateProperty(gameState->uiTextureMaterial, "zFar", &zFar);

    GlobalUniformObject globalUniformObject = {};
    globalUniformObject.viewPosition = vec3_invert_sign(gameState->camPosition);
    globalUniformObject.projView = projView;
    globalUniformObject.directionalLight = directionalLight;
    UpdateGlobalUniform(&globalUniformObject);

    if (!BeginRendering())
        return;

    RenderTargetStartRendering(gameState->shadowMapRenderTarget);

    for (int i = 0; i < DarrayGetSize(gameState->scene.vertexBufferDarray); i++)
    {
        Draw(gameState->shadowMaterial, gameState->scene.vertexBufferDarray[i], gameState->scene.indexBufferDarray[i], &gameState->scene.modelMatrixDarray[i]);
    }

    RenderTargetStopRendering(gameState->shadowMapRenderTarget);

    RenderTargetStartRendering(GetMainRenderTarget());

    for (int i = 0; i < DarrayGetSize(gameState->scene.vertexBufferDarray); i++)
    {
        Draw(gameState->lightingMaterial, gameState->scene.vertexBufferDarray[i], gameState->scene.indexBufferDarray[i], &gameState->scene.modelMatrixDarray[i]);
    }

    mat4 translation = mat4_2Dtranslate((vec2){windowAspectRatio, 1});
    mat4 rotate = mat4_rotate_x(-PI / 2);
    mat4 scale = mat4_2Dscale((vec2){windowAspectRatio, 1});
    mat4 modelMatrix = mat4_mul_mat4(translation, mat4_mul_mat4(rotate, scale));
    Draw(gameState->uiTextureMaterial, gameState->scene.vertexBufferDarray[1], gameState->scene.indexBufferDarray[1], &modelMatrix);

    RenderTargetStopRendering(GetMainRenderTarget());

    EndRendering();
}
