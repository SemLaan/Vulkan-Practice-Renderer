#pragma once
#include "lin_alg.h"

static inline f32 discriminant(f32 a, f32 b, f32 c)
{
	return b * b - 4 * a * c;
}

typedef struct QuadraticSolution
{
	f32 a;
	f32 b;
	u32 count;
} QuadraticSolution;

/// @brief Solves a quadratic
/// @param a 
/// @param b 
/// @param c 
/// @return Returns a filled out quadratic solution struct, if less than two solutions are found, b or both a and b will be left at zero for 1 and 0 solutions respectively
static inline QuadraticSolution SolveQuadratic(f32 a, f32 b, f32 c)
{
	QuadraticSolution solution = {};

	f32 D = discriminant(a, b, c);

	if (D == 0)
	{
		solution.count = 1;
		solution.a = -b / (2.f * a);
	}
	else if (D > 0)
	{
		solution.count = 2;

		D = sqrtf(D);
		solution.a = (-b + D) / (2 * a);
		solution.b = (-b - D) / (2 * a);
	}	

	return solution;
}

/// @brief Returns how far along the given ray you have to go to get to the first intersection with the circle, doesn't return intersections that happen behind the ray and returns -1 if there's no intersection
/// @param origin 
/// @param direction Ensure that the direction is normalized
/// @param center 
/// @param radius 
/// @return 
static inline f32 SolveRaySphereIntersection(vec3 origin, vec3 direction, vec3 center, f32 radius)
{
	vec3 L = vec3_sub_vec3(origin, center);
	f32 a = 1;
	f32 b = 2 * vec3_dot(L, direction);
	f32 c = vec3_dot(L, L) - radius * radius;

	QuadraticSolution solution = SolveQuadratic(a, b, c);


	if (solution.b < solution.a && solution.b > 0)
		solution.a = solution.b;

	if (solution.a > 0)
		return solution.a;

	return -1.f;
}
