#pragma once
#include "defines.h"


#define CACHE_ALIGN 64

// Copies memory from source to destination, can fail if blocks overlap
void MemoryCopy(void* destination, const void* source, size_t size);

// Sets every bit in a certain range to zero
void MemoryZero(void* block, u64 size);

// Sets each byte in an area of memory to the same value
void MemorySet(void* block, i32 value, u64 size);

// Compares the bits of two blocks of memory
bool MemoryCompare(const void* a, const void* b, u64 size);