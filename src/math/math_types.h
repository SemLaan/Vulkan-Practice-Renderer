#pragma once
#include "defines.h"

typedef struct vec2i
{
    i32 x;
    i32 y;
} vec2i;

typedef struct vec3i
{
    i32 x;
    i32 y;
    i32 z;
} vec3i;

typedef struct vec2
{
    f32 x;
    f32 y;
} vec2;

typedef struct vec3
{
    f32 x;
    f32 y;
    f32 z;
} vec3;

typedef struct vec4
{
    union {
        struct
        {
            f32 x;
            f32 y;
            f32 z;
            f32 w;
        };
        struct
        {
            f32 values[4];
        };
    };
} vec4;

typedef struct mat2
{
    f32 values[4];
} mat2;

typedef struct mat3
{
    f32 values[9];
} mat3;

typedef struct mat4
{
    f32 values[16];
} mat4;