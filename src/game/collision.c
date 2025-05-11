#include "collision.h"





RaycastHit RaycastMesh(vec3 origin, vec3 direction, MeshData mesh, mat4 modelMatrix, u32 positionOffset, u32 normalOffset)
{
	RaycastHit hit = {};
	hit.hit = false;
	hit.hitDistance = -1;
	hit.triangleFirstIndex = UINT32_MAX;

	mat4 inverseModel = mat4_inverse(modelMatrix);
	vec3 objectSpaceOrigin = mat4_mul_vec3_extend(inverseModel, origin, 1);
	vec3 objectSpaceDirection = mat4_mul_vec3_extend(inverseModel, direction, 0);
	objectSpaceDirection = vec3_normalize(objectSpaceDirection);

	u32 closestHitIValue = UINT32_MAX;
	f32 closestHitDistance = 1000000000000000;

	for (u32 i = 0; i < mesh.indexCount / 3; i++)
	{
		vec3 v0 = *(vec3*)((u8*)mesh.vertices + mesh.indices[i * 3 + 0] * mesh.vertexStride + positionOffset);
		vec3 v1 = *(vec3*)((u8*)mesh.vertices + mesh.indices[i * 3 + 1] * mesh.vertexStride + positionOffset);
		vec3 v2 = *(vec3*)((u8*)mesh.vertices + mesh.indices[i * 3 + 2] * mesh.vertexStride + positionOffset);

		// Moller Trumbore ray triangle intersection
		// https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle/moller-trumbore-ray-triangle-intersection.html
		vec3 v0v1 = vec3_sub_vec3(v1, v0);
		vec3 v0v2 = vec3_sub_vec3(v2, v0);
		vec3 P = vec3_cross_vec3(objectSpaceDirection, v0v2);
		f32 determinant = vec3_dot(v0v1, P);

		if (fabsf(determinant) < 0.00001f)
			continue;
		
		f32 inverseDeterminant = 1.f / determinant;

		vec3 T = vec3_sub_vec3(objectSpaceOrigin, v0);
		f32 u = vec3_dot(T, P) * inverseDeterminant;
		if (u < 0 || u > 1)
			continue;

		vec3 Q = vec3_cross_vec3(T, v0v1);
		f32 v = vec3_dot(objectSpaceDirection, Q) * inverseDeterminant;
		if (v < 0 || u + v > 1)
			continue;
		
		f32 t = vec3_dot(v0v2, Q) * inverseDeterminant;

		if (t < closestHitDistance)
		{
			closestHitDistance = t;
			closestHitIValue = i;
		}
	}

	if (closestHitIValue != UINT32_MAX)
	{
		hit.hit = true;
		hit.triangleFirstIndex = closestHitIValue * 3;
		hit.hitDistance = closestHitDistance;
	}

	return hit;
}


