#include "raycast_demo.h"

#include "renderer/renderer.h"
#include "renderer/material.h"
#include "math/lin_alg.h"
#include "renderer/ui/debug_ui.h"
#include "renderer/camera.h"
#include "game_rendering.h"
#include "core/input.h"

#define RAY_RENDERING_SHADER_NAME "line_shader"

typedef struct RaycastDemoState
{
	GPUMesh* raycastOriginMesh;
	Camera* sceneCamera;
	Material raycastOriginMaterial;
	Material rayRenderMaterial;
	mat4 raycastOriginModelMatrix;
	f32 sphereRadius;
	VertexT3 vertices[2];
	u32 indices[2];
	GPUMesh rayMesh;
} RaycastDemoState;

static RaycastDemoState state;

void RaycastDemoInit()
{
	ShaderCreateInfo shaderCreateInfo = {};
	shaderCreateInfo.renderTargetColor = true;
	shaderCreateInfo.renderTargetDepth = true;
	shaderCreateInfo.renderTargetStencil = false;
	shaderCreateInfo.vertexBufferLayout.perVertexAttributeCount = 3;
	shaderCreateInfo.vertexBufferLayout.perVertexAttributes[0] = VERTEX_ATTRIBUTE_TYPE_VEC3;
	shaderCreateInfo.vertexBufferLayout.perVertexAttributes[1] = VERTEX_ATTRIBUTE_TYPE_VEC3;
	shaderCreateInfo.vertexBufferLayout.perVertexAttributes[2] = VERTEX_ATTRIBUTE_TYPE_VEC2;
	shaderCreateInfo.vertexBufferLayout.perInstanceAttributeCount = 0;
	shaderCreateInfo.rasterizerMode = RASTERIZER_MODE_LINE_SEGMENTS;

	shaderCreateInfo.vertexShaderName = "default";
	shaderCreateInfo.fragmentShaderName = "default";

	ShaderCreate(RAY_RENDERING_SHADER_NAME, &shaderCreateInfo);

	state.rayRenderMaterial = MaterialCreate(ShaderGetRef(RAY_RENDERING_SHADER_NAME));

	state.raycastOriginMesh = GetBasicMesh(BASIC_MESH_NAME_SPHERE);
	state.raycastOriginMaterial = MaterialCreate(ShaderGetRef(DEFAULT_SHADER_NAME));
	state.sceneCamera = GetGameCameras().sceneCamera;
	state.sphereRadius = 80;
	VertexT3 vertex = {};
	vertex.position = vec3_create(0, 0, 0);
	vertex.normal = vec3_create(1, 0, 0);
	vertex.uvCoord = vec2_create(0, 0);
	state.vertices[0] = vertex;
	state.vertices[1] = vertex;
	state.indices[0] = 0;
	state.indices[1] = 1;
	state.rayMesh.vertexBuffer = VertexBufferCreate(state.vertices, sizeof(state.vertices));
	state.rayMesh.indexBuffer = IndexBufferCreate(state.indices, 3);
}

void RaycastDemoUpdate()
{
	if (DebugUIGetInputConsumed())
		return;

	if (GetButtonDown(BUTTON_LEFTMOUSEBTN) && !GetButtonDownPrevious(BUTTON_LEFTMOUSEBTN))
	{
		// Getting a ray from the camera in world space.
		CameraRecalculateInverseViewProjection(state.sceneCamera);
		vec4 mouseWorldPos = CameraScreenToWorldSpace(state.sceneCamera, vec2_create(GetMousePos().x, GetMousePos().y));

		state.vertices[0].position = state.sceneCamera->position;
		state.vertices[1].position = vec3_create(mouseWorldPos.x, mouseWorldPos.y, mouseWorldPos.z);
		VertexBufferUpdate(state.rayMesh.vertexBuffer, state.vertices, sizeof(state.vertices));
	}
}

void RaycastDemoRender()
{
	vec4 color = vec4_create(1, 1, 1, 1);
	MaterialUpdateProperty(state.raycastOriginMaterial, "color", &color);
	MaterialUpdateProperty(state.rayRenderMaterial, "color", &color);
	
	MaterialBind(state.raycastOriginMaterial);
	Draw(1, &state.raycastOriginMesh->vertexBuffer, state.raycastOriginMesh->indexBuffer, &state.raycastOriginModelMatrix, 1);
	mat4 identity = mat4_identity();
	MaterialBind(state.rayRenderMaterial);
	Draw(1, &state.rayMesh.vertexBuffer, state.rayMesh.indexBuffer, &identity, 1);
}

void RaycastDemoShutdown()
{
	MaterialDestroy(state.rayRenderMaterial);
	MaterialDestroy(state.raycastOriginMaterial);
	VertexBufferDestroy(state.rayMesh.vertexBuffer);
	IndexBufferDestroy(state.rayMesh.indexBuffer);
	ShaderDestroy(RAY_RENDERING_SHADER_NAME);
}


