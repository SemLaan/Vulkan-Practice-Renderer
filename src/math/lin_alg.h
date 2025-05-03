#pragma once
#include "math_types.h"

#include "core/asserts.h"
#include <immintrin.h>
// TODO: make it so this doesn't have to be included everywhere, because it is big and it's only needed for trig funcs
#include <math.h>


// !!!!!!!!!!!!!!! Matrices are stored column-wise ==========================================================

#define COL4(col_z) (col_z * 4)
#define COL3(col_z) (col_z * 3)
#define COL2(col_z) (col_z * 2)

// a b
// c d
#define M2a 0
#define M2b 2
#define M2c 1
#define M2d 3

// a b c
// d e f
// g h i
#define M3a 0
#define M3d 1
#define M3g 2
#define M3b 3
#define M3e 4
#define M3h 5
#define M3c 6
#define M3f 7
#define M3i 8


static vec2 vec2_create(f32 x, f32 y)
{
	return (vec2){x, y};
}

static vec2 vec2_add_vec2(vec2 a, vec2 b)
{
	return (vec2){a.x + b.x, a.y + b.y};
}

static f32 vec2_dot_vec2(vec2 a, vec2 b)
{
	return a.x * b.x + a.y * b.y;
}

// There is no 2d cross product, but this implements it the way it's defined in the msdf paper.
static f32 vec2_cross_vec2(vec2 a, vec2 b)
{
	return a.x * b.y - a.y * b.x;
}

static vec2 vec2_sub_vec2(vec2 a, vec2 b)
{
	return (vec2){a.x - b.x, a.y - b.y};
}

static vec2 vec2_div_float(vec2 vec, f32 value)
{
	return (vec2){ vec.x / value, vec.y / value };
}

static f32 vec2_magnitude(vec2 a)
{
	return sqrtf(a.x * a.x + a.y * a.y);
}

static vec2 vec2_normalize(vec2 a)
{
	f32 length = vec2_magnitude(a);
	return vec2_create(a.x / length, a.y / length);
}

static vec2 vec2_mul_f32(vec2 a, f32 b)
{
	return (vec2){a.x * b, a.y * b};
}

static vec3 vec4_to_vec3(vec4 v)
{
	return (vec3){v.x, v.y, v.z};
}

static vec3 vec3_from_float(f32 value)
{
	return (vec3){ value, value, value };
}

static vec3 vec3_invert_sign(vec3 v)
{
	return (vec3){-v.x, -v.y, -v.z};
}

static vec3 vec3_add_vec3(vec3 v1, vec3 v2)
{
	return (vec3){ v1.x + v2.x, v1.y + v2.y, v1.z + v2.z };
}

static vec3 vec3_sub_vec3(vec3 v1, vec3 v2)
{
	return (vec3){ v1.x - v2.x, v1.y - v2.y, v1.z - v2.z };
}

static vec3 vec3_div_float(vec3 vec, f32 value)
{
	return (vec3){ vec.x / value, vec.y / value, vec.z / value };
}

static vec3 vec3_cross_vec3(vec3 v1, vec3 v2)
{
	return (vec3){ v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x };
}

static vec3 vec3_mul_f32(vec3 a, f32 b)
{
	return (vec3){a.x * b, a.y * b, a.z * b};
}

static float vec2_distance(vec2 v1, vec2 v2)
{
	f32 deltaX = v1.x - v2.x;
	f32 deltaY = v1.y - v2.y;
	return sqrt(deltaX * deltaX + deltaY * deltaY);
}

static float vec2_distance_squared(vec2 v1, vec2 v2)
{
	f32 deltaX = v1.x - v2.x;
	f32 deltaY = v1.y - v2.y;
	return deltaX * deltaX + deltaY * deltaY;
}

static float vec3_distance(vec3 v1, vec3 v2)
{
	f32 deltaX = v1.x - v2.x;
	f32 deltaY = v1.y - v2.y;
	f32 deltaZ = v1.z - v2.z;
	return sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);
}

static float vec3_distance_squared(vec3 v1, vec3 v2)
{
	f32 deltaX = v1.x - v2.x;
	f32 deltaY = v1.y - v2.y;
	f32 deltaZ = v1.z - v2.z;
	return deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ;
}

static vec3 vec3_lerp(vec3 v1, vec3 v2, f32 t)
{
	return vec3_add_vec3(v1, vec3_mul_f32(vec3_sub_vec3(v2, v1), t));
}

static vec3 vec3_create(f32 x, f32 y, f32 z)
{
	vec3 out;
	out.x = x;
	out.y = y;
	out.z = z;
	return out;
}

static f32 vec3_magnitude(vec3 a)
{
	return sqrtf(a.x * a.x + a.y * a.y + a.z * a.z);
}

static vec3 vec3_normalize(vec3 a)
{
	f32 length = vec3_magnitude(a);
	return vec3_create(a.x / length, a.y / length, a.z / length);
}

static vec4 vec4_create(f32 x, f32 y, f32 z, f32 w)
{
	vec4 out;
	out.x = x;
	out.y = y;
	out.z = z;
	out.w = w;
	return out;
}

static vec4 vec4_mul_f32(vec4 a, f32 b)
{
	return vec4_create(a.x * b, a.y * b, a.z * b, a.w * b);
}

static f32 vec4_dot(vec4 v1, vec4 v2)
{
	return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z + v1.w * v2.w;
}

static vec4 vec4_sub_vec4(vec4 a, vec4 b)
{
	return vec4_create(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

static vec2 vec4_xy(vec4 v)
{
	return vec2_create(v.x, v.y);
}

static mat2 mat2_identity()
{
	mat2 mat = {};
	mat.values[M2a] = 1.f;
	mat.values[M2d] = 1.f;
	return mat;
}

static mat3 mat3_identity()
{
	mat3 mat = {};
	mat.values[M3a] = 1.f;
	mat.values[M3e] = 1.f;
	mat.values[M3i] = 1.f;
	return mat;
}

static mat4 mat4_identity()
{
	mat4 mat = {};

	mat.values[0] = 1.f;
	mat.values[1 + COL4(1)] = 1.f;
	mat.values[2 + COL4(2)] = 1.f;
	mat.values[3 + COL4(3)] = 1.f;

	return mat;
}

static mat4 mat4_3Dscale(vec3 vec)
{
	mat4 mat = {};

	mat.values[0] = vec.x;
	mat.values[1 + COL4(1)] = vec.y;
	mat.values[2 + COL4(2)] = vec.z;
	mat.values[3 + COL4(3)] = 1.f;

	return mat;
}

static mat4 mat4_2Dscale(vec2 vec)
{
	mat4 mat = {};

	mat.values[0] = vec.x;
	mat.values[1 + COL4(1)] = vec.y;
	mat.values[2 + COL4(2)] = 1.f;
	mat.values[3 + COL4(3)] = 1.f;

	return mat;
}

static mat4 mat4_3Dtranslate(vec3 vec)
{
	mat4 mat = {};

	mat.values[0] = 1.f;
	mat.values[1 + COL4(1)] = 1.f;
	mat.values[2 + COL4(2)] = 1.f;
	mat.values[3 + COL4(3)] = 1.f;

	mat.values[0 + COL4(3)] = vec.x;
	mat.values[1 + COL4(3)] = vec.y;
	mat.values[2 + COL4(3)] = vec.z;

	return mat;
}

static mat4 mat4_2Dtranslate(vec2 vec)
{
	mat4 mat = {};

	mat.values[0] = 1.f;
	mat.values[1 + COL4(1)] = 1.f;
	mat.values[2 + COL4(2)] = 1.f;
	mat.values[3 + COL4(3)] = 1.f;

	mat.values[0 + COL4(3)] = vec.x;
	mat.values[1 + COL4(3)] = vec.y;

	return mat;
}

static mat4 mat4_mul_mat4(mat4 a, mat4 b)
{
	mat4 result = {};

	for (u32 i = 0; i < 4; i++)
	{
		for (u32 j = 0; j < 4; j++)
		{
			result.values[i + COL4(j)] = a.values[i + COL4(0)] * b.values[0 + COL4(j)] + a.values[i + COL4(1)] * b.values[1 + COL4(j)] + a.values[i + COL4(2)] * b.values[2 + COL4(j)] + a.values[i + COL4(3)] * b.values[3 + COL4(j)];
		}
	}

	return result;
}

static mat4 mat4_rotate_x(f32 angle_radians) 
{
	f32 c = (f32)cos(angle_radians);
	f32 s = (f32)sin(angle_radians);

	mat4 out_matrix = mat4_identity();
	out_matrix.values[1 + COL4(1)] = c;
	out_matrix.values[1 + COL4(2)] = s;
	out_matrix.values[2 + COL4(1)] = -s;
	out_matrix.values[2 + COL4(2)] = c;
	return out_matrix;
}

static mat4 mat4_rotate_y(f32 angle_radians) 
{
	f32 c = (f32)cos(angle_radians);
	f32 s = (f32)sin(angle_radians);

	mat4 out_matrix = mat4_identity();
	out_matrix.values[0 + COL4(0)] = c;
	out_matrix.values[0 + COL4(2)] = -s;
	out_matrix.values[2 + COL4(0)] = s;
	out_matrix.values[2 + COL4(2)] = c;
	return out_matrix;
}

static mat4 mat4_rotate_z(f32 angle_radians) 
{
	f32 c = (f32)cos(angle_radians);
	f32 s = (f32)sin(angle_radians);

	mat4 out_matrix = mat4_identity();
	out_matrix.values[0 + COL4(0)] = c;
	out_matrix.values[0 + COL4(1)] = s;
	out_matrix.values[1 + COL4(0)] = -s;
	out_matrix.values[1 + COL4(1)] = c;
	return out_matrix;
}

static mat4 mat4_rotate_xyz(vec3 angles)
{
	mat4 rx = mat4_rotate_x(angles.x);
	mat4 ry = mat4_rotate_y(angles.y);
	mat4 rz = mat4_rotate_z(angles.z);
	mat4 out_matrix = mat4_mul_mat4(rx, ry);
	out_matrix = mat4_mul_mat4(out_matrix, rz);
	return out_matrix;
}

static mat4 mat4_transpose(mat4 mat)
{
	mat4 transposed = mat;

	for (u32 i = 0; i < 4; ++i)
	{
		for (u32 j = 0; j < 4; ++j)
		{
			transposed.values[j + COL4(i)] = mat.values[i + COL4(j)];
		}
	}

	return transposed;
}

static mat4 mat4_perspective(f32 verticalFovDegrees, f32 aspectRatio, f32 near, f32 far)
{
	f32 verticalFovRadians = verticalFovDegrees * 2.0f * PI / 360.0f;
	f32 focalLength = 1.0f / (f32)tan(verticalFovRadians / 2.0f);

	f32 x = focalLength / aspectRatio;
	f32 y = focalLength;
	f32 A = near / (far - near);
	f32 B = far * A;

	mat4 projection = {};

	projection.values[0 + COL4(0)] = x;
	projection.values[1 + COL4(1)] = -y;
	projection.values[2 + COL4(2)] = A;
	projection.values[2 + COL4(3)] = B;
	projection.values[3 + COL4(3)] = 0.f;
	projection.values[3 + COL4(2)] = -1.f;

	return projection;
}

static mat4 mat4_orthographic(f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far)
{
	mat4 projection = {};

	projection.values[0 + COL4(0)] = 2.f / (right - left);
	projection.values[1 + COL4(1)] = -2.f / (top - bottom);
	projection.values[2 + COL4(2)] = 1.f / (far - near);
	projection.values[3 + COL4(3)] = 1.f;
	projection.values[0 + COL4(3)] = -(right + left) / (right - left);
	projection.values[1 + COL4(3)] = (top + bottom) / (top - bottom);
	projection.values[2 + COL4(3)] = near / (far - near) + 1;

	return projection;
}

static f32 mat2_determinant(mat2 A)
{
	// A:
	// a b
	// c d
	// determinant = ad - bc
	return A.values[M2a] * A.values[M2d] - A.values[M2b] * A.values[M2c];
}

static f32 mat3_determinant(mat3 A)
{
	// A:
	// a b c
	// d e f
	// g h i
	// Determinant = a * minor(a) - b * minor(b) + c * minor(c)
	f32 accumulator = 0;

	mat2 temp = {};
	temp.values[M2a] = A.values[M3e];
	temp.values[M2b] = A.values[M3f];
	temp.values[M2c] = A.values[M3h];
	temp.values[M2d] = A.values[M3i];
	accumulator += A.values[M3a] * mat2_determinant(temp);

	temp.values[M2a] = A.values[M3d];
	temp.values[M2c] = A.values[M3g];
	accumulator -= A.values[M3b] * mat2_determinant(temp);

	temp.values[M2b] = A.values[M3e];
	temp.values[M2d] = A.values[M3h];
	accumulator += A.values[M3c] * mat2_determinant(temp);

	return accumulator;
}

static mat4 mat4_inverse(mat4 A)
{
	mat4 adjoint = {};

	i32 sign = 1;

	// Calculate adjoint
	for (u32 k = 0; k < 4; ++k)
	{
		for (u32 l = 0; l < 4; ++l)
		{
			// Get matrix with deleted row and column to calculate the minor of A[k, l]
			u32 row = 0, col = 0; // Row and col of the smaller matrix
			mat3 temp = {};
			for (u32 i = 0; i < 4; ++i)
			{
				if (i == k) continue;
				for (u32 j = 0; j < 4; ++j)
				{
					if (j == l) continue;

					temp.values[row + COL3(col)] = A.values[i + COL4(j)];
					col++;
				}
				col = 0;
				row++;
			}

			// Get determinant of small matrix
			f32 determinant = mat3_determinant(temp);
			// Calculate the minor of A[k, l], multiplying it by the correct sign and inserting it in adjoint[l, k] so it's already transposed since the adjoint is the transpose of the cofactor
			adjoint.values[l + COL4(k)] = sign * determinant;

			sign = -sign;
		}
		sign = -sign;
	}

	f32 determinant = A.values[0 + COL4(0)] * adjoint.values[0 + COL4(0)] + 
					  A.values[0 + COL4(1)] * adjoint.values[1 + COL4(0)] + 
					  A.values[0 + COL4(2)] * adjoint.values[2 + COL4(0)] + 
					  A.values[0 + COL4(3)] * adjoint.values[3 + COL4(0)];

	//GRASSERT_MSG(abs((0) - (determinant)) > 0.001f, "determinant was zero, matrix is singular");

	// divide adjoint by determinant elementwise to get inverse
	f32 inverseDeterminant = 1.f / determinant;
	for (u32 i = 0; i < 16/*elements of 4x4 mat*/; ++i)
	{
		adjoint.values[i] = adjoint.values[i] * inverseDeterminant;
	}

	// Return variable adjoint as adjoint was just overriden with the inverse
	return adjoint;
}

static vec4 mat4_mul_vec4(mat4 A, vec4 b)
{
	vec4 result = {};

	for (u32 i = 0; i < 4; ++i)
	{
		result.values[i] = A.values[i + COL4(0)] * b.values[0] + A.values[i + COL4(1)] * b.values[1] + A.values[i + COL4(2)] * b.values[2] + A.values[i + COL4(3)] * b.values[3];
	}

	return result;
}