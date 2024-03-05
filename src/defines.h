#pragma once
#include <stdint.h>
#include <stdbool.h>


// 16 bit int types are commented out because they are rarely used and slower than 32 bit according to my stdint.h implementation

// Unsigned int types.
typedef uint8_t u8;
//typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

// Signed int types.
typedef int8_t i8;
//typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

// Floating point types
typedef float f32;
typedef double f64;

// Boolean types
typedef uint32_t b32;

#define KiB (1024ULL)
#define MiB (1024ULL * 1024ULL)
#define GiB (1024ULL * 1024ULL * 1024ULL)

#define nullptr 0

#define MIN_ALIGNMENT 4