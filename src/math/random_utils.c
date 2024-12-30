#include "random_utils.h"
#include "math.h"
#include "lin_alg.h"

vec2 RandomPointOnUnitDisc(u32* seed)
{
	f32 randomAngle = RandomFloat(seed) * PI*2;
	return vec2_create(cos(randomAngle), sin(randomAngle));
}

vec2 RandomPointInUnitDisc(u32* seed)
{
	f32 randomAngle = RandomFloat(seed) * PI*2;
	f32 randomRadius = sqrt(RandomFloat(seed));
	vec2 pointOnUnitDisc = vec2_create(cos(randomAngle), sin(randomAngle));
	return vec2_mul_f32(pointOnUnitDisc, randomRadius);
}

vec3 RandomPointOnUnitSphere(u32* seed)
{
	f32 randomTheta = 2*PI * RandomFloat(seed);
	f32 randomPhi = acos(2 * RandomFloat(seed) - 1) - 0.5f*PI;
	f32 y = sin(randomPhi);
	f32 x = cos(randomPhi) * sin(randomTheta);
	f32 z = cos(randomPhi) * cos(randomTheta);
	return vec3_create(x, y, z);
}

vec3 RandomPointInUnitSphere(u32* seed)
{
	f32 randomTheta = 2*PI * RandomFloat(seed);
	f32 randomPhi = acos(2 * RandomFloat(seed) - 1) - 0.5f*PI;
	f32 randomRadius = pow(RandomFloat(seed), 1/3);
	f32 y = sin(randomPhi);
	f32 x = cos(randomPhi) * sin(randomTheta);
	f32 z = cos(randomPhi) * cos(randomTheta);
	return vec3_mul_f32(vec3_create(x, y, z), randomRadius);
}


