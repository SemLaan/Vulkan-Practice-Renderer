#include "mem_utils.h"

#include <string.h>

void MemoryCopy(void* destination, const void* source, size_t size)
{
	memcpy(destination, source, size);
}

void MemoryZero(void* block, u64 size)
{
	memset(block, 0, size);
}

void MemorySet(void* block, i32 value, u64 size)
{
	memset(block, value, size);
}

bool MemoryCompare(void* a, void* b, u64 size)
{
	return 0 == memcmp(a, b, size);
}