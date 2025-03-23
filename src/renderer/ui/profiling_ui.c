#include "profiling_ui.h"
#include "renderer/renderer.h"
#include "renderer/ui/text_renderer.h"
#include "math/lin_alg.h"
#include "core/event.h"
#include "core/platform.h"

#define FRAME_STATS_BACKGROUND_SHADER_NAME "flat_color_shader"
#define PROFILING_UI_FONT_NAME "profiling_font"

typedef struct ProfilingUIState
{
	Material flatWhiteMaterial;
	Material flatBlackMaterial;
	TextBatch* frameStatsTextBatch;
	MeshData* quadMesh;
	mat4 projection;
} ProfilingUIState;

static ProfilingUIState* state = nullptr;

static bool OnWindowResize(EventCode type, EventData data)
{
    // Recalculating projection matrix
    vec2i windowSize = GetPlatformWindowSize();
    f32 windowAspectRatio = windowSize.x / (f32)windowSize.y;
    state->projection = mat4_orthographic(0, 10 * windowAspectRatio, 0, 10, -1, 1);

    return false;
}

void InitializeProfilingUI()
{
	state = Alloc(GetGlobalAllocator(), sizeof(*state));

	ShaderCreateInfo shaderCreateInfo = {};
    shaderCreateInfo.vertexShaderName = "ui_flat";
    shaderCreateInfo.fragmentShaderName = "ui_flat";
    shaderCreateInfo.vertexBufferLayout.perVertexAttributeCount = 3;
    shaderCreateInfo.vertexBufferLayout.perVertexAttributes[0] = VERTEX_ATTRIBUTE_TYPE_VEC3;
    shaderCreateInfo.vertexBufferLayout.perVertexAttributes[1] = VERTEX_ATTRIBUTE_TYPE_VEC3;
    shaderCreateInfo.vertexBufferLayout.perVertexAttributes[2] = VERTEX_ATTRIBUTE_TYPE_VEC2;
    shaderCreateInfo.renderTargetColor = true;
    shaderCreateInfo.renderTargetDepth = false;
    shaderCreateInfo.renderTargetStencil = false;
    ShaderCreate(FRAME_STATS_BACKGROUND_SHADER_NAME, &shaderCreateInfo);

	state->quadMesh = GetBasicMesh(BASIC_MESH_NAME_QUAD);
	TextLoadFont(PROFILING_UI_FONT_NAME, "Roboto-Black.ttf");
	state->frameStatsTextBatch = TextBatchCreate(PROFILING_UI_FONT_NAME);
	state->flatWhiteMaterial = MaterialCreate(ShaderGetRef(FRAME_STATS_BACKGROUND_SHADER_NAME));
	state->flatBlackMaterial = MaterialCreate(ShaderGetRef(FRAME_STATS_BACKGROUND_SHADER_NAME));

	vec2i windowSize = GetPlatformWindowSize();
    f32 windowAspectRatio = windowSize.x / (f32)windowSize.y;
    state->projection = mat4_orthographic(0, 10 * windowAspectRatio, 0, 10, -1, 1);

	RegisterEventListener(EVCODE_WINDOW_RESIZED, OnWindowResize);
}

void ShutdownProfilingUI()
{
	UnregisterEventListener(EVCODE_WINDOW_RESIZED, OnWindowResize);

	MaterialDestroy(state->flatBlackMaterial);
	MaterialDestroy(state->flatWhiteMaterial);
	TextBatchDestroy(state->frameStatsTextBatch);
	TextUnloadFont(PROFILING_UI_FONT_NAME);

	Free(GetGlobalAllocator(), state);
}

void UpdateProfilingUI()
{
	vec4 white = vec4_create(1, 1, 1, 1);
	vec4 black = vec4_create(0, 0, 0, 1);
    MaterialUpdateProperty(state->flatWhiteMaterial, "color", &white);
    MaterialUpdateProperty(state->flatBlackMaterial, "color", &black);
}

void DrawFrameStats()
{
	const f32 orthoHeigh = 10;
	const f32 blockHeight = 0.2f;
	const f32 blockWidth = 3.f;
	const f32 whiteBorderThickness = 0.01f;
	const f32 blackYPos = orthoHeigh - (blockHeight + whiteBorderThickness);
	const f32 whiteYPos = orthoHeigh - (blockHeight + whiteBorderThickness * 2);

	mat4 modelBlack = mat4_mul_mat4(state->projection, mat4_mul_mat4(mat4_2Dtranslate(vec2_create(whiteBorderThickness, blackYPos)), mat4_2Dscale(vec2_create(blockWidth, blockHeight))));
	mat4 modelWhite = mat4_mul_mat4(state->projection, mat4_mul_mat4(mat4_2Dtranslate(vec2_create(0, whiteYPos)), mat4_2Dscale(vec2_create(blockWidth + whiteBorderThickness * 2, blockHeight + whiteBorderThickness * 2))));
	MaterialBind(state->flatWhiteMaterial);
	Draw(1, &state->quadMesh->vertexBuffer, state->quadMesh->indexBuffer, &modelWhite, 1);
	MaterialBind(state->flatBlackMaterial);
	Draw(1, &state->quadMesh->vertexBuffer, state->quadMesh->indexBuffer, &modelBlack, 1);

	//TextBatchRender(state->frameStatsTextBatch, );
}


