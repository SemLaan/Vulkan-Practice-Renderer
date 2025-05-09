#include "raycast_demo.h"

#include "renderer/renderer.h"
#include "renderer/material.h"
#include "math/lin_alg.h"
#include "renderer/ui/debug_ui.h"
#include "renderer/camera.h"
#include "game_rendering.h"
#include "core/input.h"

#define RAY_RENDERING_SHADER_NAME "line_shader"
#define RAY_ORBIT_DISTANCE 55
#define RAY_ORB_SIZE 2.f

typedef struct RaycastDemoState
{
	GPUMesh* raycastOriginMesh;
	Camera* sceneCamera;
	Material raycastOriginMaterial;
	Material rayRenderMaterial;
	mat4 raycastOriginModelMatrix;
	VertexT3 vertices[2];
	u32 indices[2];
	GPUMesh rayMesh;
	vec3 rayOrbPosition;
	bool movingRayOrb;
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
	VertexT3 vertex = {};
	vertex.position = vec3_create(0, 0, 0);
	vertex.normal = vec3_create(1, 0, 0);
	vertex.uvCoord = vec2_create(0, 0);
	state.vertices[0] = vertex;
	state.vertices[1] = vertex;
	state.rayOrbPosition = vec3_create(RAY_ORBIT_DISTANCE, 0, 0);
	state.vertices[1].position = state.rayOrbPosition;
	state.indices[0] = 0;
	state.indices[1] = 1;
	state.rayMesh.vertexBuffer = VertexBufferCreate(state.vertices, sizeof(state.vertices));
	state.rayMesh.indexBuffer = IndexBufferCreate(state.indices, 3);
	state.movingRayOrb = false;
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

		// Calculating if the player clicked the ray origin orb
		vec3 rayOrigin = state.sceneCamera->position;
		vec3 rayDirection = vec3_sub_vec3(vec3_create(mouseWorldPos.x, mouseWorldPos.y, mouseWorldPos.z), rayOrigin);
		vec3 L = vec3_sub_vec3(rayOrigin, state.rayOrbPosition);
		f32 a = vec3_dot(rayDirection, rayDirection);
		f32 b = 2.f * vec3_dot(L, rayDirection);
		f32 c = vec3_dot(L, L) - RAY_ORB_SIZE * RAY_ORB_SIZE;

		f32 discriminant = b * b - 4 * a * c;

		if (discriminant > 0)
		{
			state.movingRayOrb = true;
		}
	}

	if (state.movingRayOrb)
	{
		// Getting a ray from the camera in world space.
		CameraRecalculateInverseViewProjection(state.sceneCamera);
		vec4 mouseWorldPos = CameraScreenToWorldSpace(state.sceneCamera, vec2_create(GetMousePos().x, GetMousePos().y));

		// Calculating if the player clicked the ray origin orb
		vec3 rayOrigin = state.sceneCamera->position;
		vec3 rayDirection = vec3_normalize(vec3_sub_vec3(vec3_create(mouseWorldPos.x, mouseWorldPos.y, mouseWorldPos.z), rayOrigin));
		vec3 L = rayOrigin;
		f32 a = vec3_dot(rayDirection, rayDirection);
		f32 b = 2.f * vec3_dot(L, rayDirection);
		f32 c = vec3_dot(L, L) - RAY_ORBIT_DISTANCE * RAY_ORBIT_DISTANCE;

		f32 discriminant = b * b - 4 * a * c;

		if (discriminant > 0)
		{
			discriminant = sqrtf(discriminant);
			f32 t1 = (-b + discriminant) / 2 * a;
			f32 t2 = (-b - discriminant) / 2 * a;

			if (t2 < t1 && t2 > 0)
				t1 = t2;

			if (t1 > 0)
			{
				state.rayOrbPosition = vec3_add_vec3(rayOrigin, vec3_mul_f32(rayDirection, t1));
			}
		}

		state.vertices[1].position = state.rayOrbPosition;
		VertexBufferUpdate(state.rayMesh.vertexBuffer, state.vertices, sizeof(state.vertices));

		if (!GetButtonDown(BUTTON_LEFTMOUSEBTN))
			state.movingRayOrb = false;
	}
}

void RaycastDemoRender()
{
	vec4 color = vec4_create(1, 1, 1, 1);
	MaterialUpdateProperty(state.raycastOriginMaterial, "color", &color);
	MaterialUpdateProperty(state.rayRenderMaterial, "color", &color);

	mat4 raycastOriginModelMatrix = mat4_mul_mat4(mat4_3Dtranslate(state.rayOrbPosition), mat4_3Dscale(vec3_from_float(RAY_ORB_SIZE)));
	
	MaterialBind(state.raycastOriginMaterial);
	Draw(1, &state.raycastOriginMesh->vertexBuffer, state.raycastOriginMesh->indexBuffer, &raycastOriginModelMatrix, 1);
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


