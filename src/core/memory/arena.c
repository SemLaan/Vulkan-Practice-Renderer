#include "arena.h"

#include "../asserts.h"


#define DEFAULT_ALIGNMENT 4

Arena ArenaCreate(Allocator* allocator, size_t size)
{
	Arena arena = {};
	arena.memoryBlock = Alloc(allocator, size, MEM_TAG_TEST);
	arena.arenaPointer = arena.memoryBlock;
	arena.arenaCapacity = size;
}

void ArenaDestroy(Arena* arena, Allocator* allocator)
{
	Free(allocator, arena->memoryBlock);
	arena->memoryBlock = nullptr;
}

void* ArenaAlloc(Arena* arena, size_t allocSize)
{
	GRASSERT_DEBUG(arena->memoryBlock);
	void* allocation = ((size_t)arena->arenaPointer + DEFAULT_ALIGNMENT - 1) & ~(DEFAULT_ALIGNMENT - 1);
	arena->arenaPointer = (size_t)allocation + allocSize;
	GRASSERT((size_t)arena->memoryBlock + arena->arenaCapacity > (size_t)arena->arenaPointer);
	return allocation;
}

void* ArenaAlignedAlloc(Arena* arena, size_t allocSize, size_t allocAlignment)
{
	GRASSERT_DEBUG(arena->memoryBlock);
	void* allocation = ((size_t)arena->arenaPointer + allocAlignment - 1) & ~(allocAlignment - 1);
	arena->arenaPointer = (size_t)allocation + allocSize;
	GRASSERT((size_t)arena->memoryBlock + arena->arenaCapacity > (size_t)arena->arenaPointer);
	return allocation;
}

void ArenaClear(Arena* arena)
{
	arena->arenaPointer = arena->memoryBlock;
}


