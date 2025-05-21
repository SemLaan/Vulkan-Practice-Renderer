#include "raycast_demo.h"

#include "renderer/renderer.h"
#include "renderer/material.h"
#include "math/lin_alg.h"
#include "renderer/ui/debug_ui.h"
#include "renderer/camera.h"
#include "game_rendering.h"
#include "core/input.h"
#include "collision.h"
#include "world_generation.h"
#include "core/platform.h"

#define RAY_RENDERING_SHADER_NAME "line_shader"
#define RAY_ORBIT_DISTANCE 55
#define RAY_ORB_SIZE 2.f
#define RAY_VERTEX_COUNT 2
#define TRIANGLE_VERTEX_COUNT 3

typedef struct RaycastDemoState
{
	GPUMesh* raycastOriginMesh;
	Camera* sceneCamera;
	Material raycastOriginMaterial;
	Material rayRenderMaterial;
	mat4 raycastOriginModelMatrix;
	VertexT3 rayVertices[RAY_VERTEX_COUNT];
	GPUMesh rayMesh;
	VertexT3 triangleVertices[TRIANGLE_VERTEX_COUNT];
	GPUMesh triangleMesh;
	vec3 rayOrbPosition;
	bool movingRayOrb;
	bool rayHitting;
} RaycastDemoState;

static RaycastDemoState state;

static inline void CalculateRayMeshIntersect();

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

	// Initializing ray visualisation
	VertexT3 vertex = {};
	vertex.position = vec3_create(0, 0, 0);
	vertex.normal = vec3_create(1, 0, 0);
	vertex.uvCoord = vec2_create(0, 0);
	state.rayVertices[0] = vertex;
	state.rayVertices[1] = vertex;
	state.rayOrbPosition = vec3_create(RAY_ORBIT_DISTANCE, 0, 0);
	state.rayVertices[1].position = state.rayOrbPosition;
	u32 indices[TRIANGLE_VERTEX_COUNT];	// This array is used to initialize the index buffer of the ray and of the triangle which is why it is size 3
	indices[0] = 0;
	indices[1] = 1;
	indices[2] = 2;	// Only for triangle IB
	state.rayMesh.vertexBuffer = VertexBufferCreate(state.rayVertices, sizeof(state.rayVertices));
	state.rayMesh.indexBuffer = IndexBufferCreate(indices, RAY_VERTEX_COUNT);
	state.movingRayOrb = false;

	// Initializing ray hit indicator
	state.triangleVertices[0] = vertex;
	state.triangleVertices[1] = vertex;
	state.triangleVertices[2] = vertex;
	state.triangleMesh.vertexBuffer = VertexBufferCreate(state.triangleVertices, sizeof(state.triangleVertices));
	state.triangleMesh.indexBuffer = IndexBufferCreate(indices, TRIANGLE_VERTEX_COUNT);
	CalculateRayMeshIntersect();
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

		// If the user hits the invisible sphere, simply find the intersection point and put the ray orb there
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
		else // If the user aimed somewhere outside the sphere, calculate the tangent line of the sphere that goes through 
			 // the camera position and has a direction that is closest to the direction of the camera ray
		{
			// Sphere sphere intersection
			// https://gamedev.stackexchange.com/questions/75756/sphere-sphere-intersection-and-circle-sphere-intersection
			f32 distance = vec3_magnitude(rayOrigin); // The main sphere's center is at 0, 0, 0 so the distance from the camera is just the length of ray origin

			// Sphere one is the ray orbit sphere, sphere two is the sphere that originates at the camera and has a radius so it passes through the center of sphere one

			// Original formula in the answer can be simplified since we only care about a specific case where the center of sphere two is always outside sphere one 
			// and its radius is the same as the distance between the two centers so that it passes through the center of sphere one
			//f32 h = 1/2 + (RAY_ORBIT_DISTANCE * RAY_ORBIT_DISTANCE - distance * distance)/(2 * distance * distance);
			f32 h = (RAY_ORBIT_DISTANCE * RAY_ORBIT_DISTANCE)/(2 * distance * distance); // h is the interpolation value to get from the centers of the spheres to the center of the intersection (c_i = c_1 + h * (c_2 - c_1))

			f32 intersectionRadius = sqrt(RAY_ORBIT_DISTANCE * RAY_ORBIT_DISTANCE - h*h*distance*distance);

			// Formula can be simplified again because sphere one is always at 0,0,0
			//vec3 intersectionCenter = vec3_add_vec3(vec3_create(0, 0, 0), vec3_mul_f32(vec3_sub_vec3(rayOrigin, vec3_create(0, 0, 0)), h));
			vec3 intersectionCenter = vec3_mul_f32(rayOrigin, h);

			vec2 mouseScreenPosition = vec2_create(GetMousePos().x, GetMousePos().y);
			vec2i windowSize = GetPlatformWindowSize();

    		mouseScreenPosition.x = mouseScreenPosition.x / windowSize.x;
    		mouseScreenPosition.y = mouseScreenPosition.y / windowSize.y;
    		mouseScreenPosition.x = mouseScreenPosition.x * 2;
    		mouseScreenPosition.y = mouseScreenPosition.y * 2;
    		mouseScreenPosition.x -= 1;
    		mouseScreenPosition.y -= 1;
			mouseScreenPosition = vec2_normalize(mouseScreenPosition);

			vec3 tangent = vec3_add_vec3(vec3_mul_f32(CameraGetRight(state.sceneCamera), mouseScreenPosition.x), vec3_mul_f32(CameraGetUp(state.sceneCamera), mouseScreenPosition.y));
			
			vec3 intersection = vec3_add_vec3(intersectionCenter, vec3_mul_f32(tangent, intersectionRadius));
			vec3 intersection2 = vec3_add_vec3(intersectionCenter, vec3_mul_f32(tangent, -intersectionRadius));

			f32 alignmentValue1 = vec3_dot(rayDirection, intersection);
			f32 alignmentValue2 = vec3_dot(rayDirection, intersection2);

			if (alignmentValue2 > alignmentValue1)
			{
				intersection = intersection2;
			}
		}

		state.rayVertices[0].position = vec3_mul_f32(vec3_sub_vec3(vec3_create(0, 0, 0), state.rayOrbPosition), 100);
		state.rayVertices[1].position = state.rayOrbPosition;
		VertexBufferUpdate(state.rayMesh.vertexBuffer, state.rayVertices, sizeof(state.rayVertices));

		CalculateRayMeshIntersect();

		if (!GetButtonDown(BUTTON_LEFTMOUSEBTN))
			state.movingRayOrb = false;
	}
}

void RaycastDemoRender()
{
	vec4 color = vec4_create(1, 1, 1, 1);
	vec4 rayColor = vec4_create(1, 1, 1, 1);
	if (state.rayHitting)
	{
		vec3 normal = vec3_normalize(vec3_cross_vec3(vec3_sub_vec3(state.triangleVertices[1].position, state.triangleVertices[0].position), 
													 vec3_sub_vec3(state.triangleVertices[2].position, state.triangleVertices[0].position)));
		
		color = vec4_add_vec4(vec4_mul_f32(vec4_create(normal.x, normal.y, normal.z, 1), 0.5f), vec4_create(0.5f, 0.5f, 0.5f, 0.5f));
		//rayColor = vec4_add_vec4(vec4_mul_f32(vec4_sub_vec4(rayColor, vec4_create(0.5f, 0.5f, 0.5f, 0.0f)), -1.f), vec4_create(0.5f, 0.5f, 0.5f, 2.0f));
	}
	MaterialUpdateProperty(state.raycastOriginMaterial, "color", &color);
	MaterialUpdateProperty(state.rayRenderMaterial, "color", &rayColor);

	mat4 raycastOriginModelMatrix = mat4_mul_mat4(mat4_3Dtranslate(state.rayOrbPosition), mat4_3Dscale(vec3_from_float(RAY_ORB_SIZE)));
	MaterialBind(state.raycastOriginMaterial);
	Draw(1, &state.raycastOriginMesh->vertexBuffer, state.raycastOriginMesh->indexBuffer, &raycastOriginModelMatrix, 1);

	if (state.rayHitting)
	{
		mat4 worldModelMatrix = WorldGenerationGetModelMatrix();
		Draw(1, &state.triangleMesh.vertexBuffer, state.triangleMesh.indexBuffer, &worldModelMatrix, 1);
	}

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
	VertexBufferDestroy(state.triangleMesh.vertexBuffer);
	IndexBufferDestroy(state.triangleMesh.indexBuffer);
	ShaderDestroy(RAY_RENDERING_SHADER_NAME);
}

static inline void CalculateRayMeshIntersect()
{
	MeshData colliderMesh = WorldGenerationGetColliderMesh();
	vec3 origin = state.rayVertices[1].position;
	vec3 direction = vec3_normalize(vec3_sub_vec3(state.rayVertices[0].position, state.rayVertices[1].position));
	mat4 model = WorldGenerationGetModelMatrix();
	RaycastHit hit = RaycastMesh(origin, direction, colliderMesh, model, offsetof(VertexT2, position), offsetof(VertexT2, normal));

	state.rayHitting = hit.hit;
	if (hit.hit)
	{
		VertexT2* colliderVertices = colliderMesh.vertices;
		state.triangleVertices[0].position = vec3_add_vec3(colliderVertices[colliderMesh.indices[hit.triangleFirstIndex + 0]].position, vec3_mul_f32(colliderVertices[colliderMesh.indices[hit.triangleFirstIndex + 0]].normal, -0.01f));
		state.triangleVertices[1].position = vec3_add_vec3(colliderVertices[colliderMesh.indices[hit.triangleFirstIndex + 1]].position, vec3_mul_f32(colliderVertices[colliderMesh.indices[hit.triangleFirstIndex + 1]].normal, -0.01f));
		state.triangleVertices[2].position = vec3_add_vec3(colliderVertices[colliderMesh.indices[hit.triangleFirstIndex + 2]].position, vec3_mul_f32(colliderVertices[colliderMesh.indices[hit.triangleFirstIndex + 2]].normal, -0.01f));
		VertexBufferUpdate(state.triangleMesh.vertexBuffer, state.triangleVertices, sizeof(state.triangleVertices));
	}

	_DEBUG("Hit triangle: %u", hit.triangleFirstIndex);
	_DEBUG("Distance: %f", hit.hitDistance);
}

