#include "arena.h"

#include "../asserts.h"


#define DEFAULT_ALIGNMENT 4

Arena ArenaCreate(Allocator* allocator, size_t size)
{
	Arena arena = {};
	arena.memoryBlock = Alloc(allocator, size);
	arena.arenaPointer = arena.memoryBlock;
	arena.arenaCapacity = size;
	return arena;
}

void ArenaDestroy(Arena* arena, Allocator* allocator)
{
	Free(allocator, arena->memoryBlock);
	arena->memoryBlock = nullptr;
}

void* ArenaAlloc(Arena* arena, size_t allocSize)
{
	GRASSERT_DEBUG(arena->memoryBlock);
	void* allocation = (void*)(((size_t)arena->arenaPointer + DEFAULT_ALIGNMENT - 1) & ~(DEFAULT_ALIGNMENT - 1));
	arena->arenaPointer = (void*)((size_t)allocation + allocSize);
	GRASSERT((size_t)arena->memoryBlock + arena->arenaCapacity > (size_t)arena->arenaPointer);
	return allocation;
}

void* ArenaAlignedAlloc(Arena* arena, size_t allocSize, size_t allocAlignment)
{
	GRASSERT_DEBUG(arena->memoryBlock);
	void* allocation = (void*)(((size_t)arena->arenaPointer + allocAlignment - 1) & ~(allocAlignment - 1));
	arena->arenaPointer = (void*)((size_t)allocation + allocSize);
	GRASSERT((size_t)arena->memoryBlock + arena->arenaCapacity > (size_t)arena->arenaPointer);
	return allocation;
}

void ArenaClear(Arena* arena)
{
	arena->arenaPointer = arena->memoryBlock;
}

ArenaMarker ArenaGetMarker(Arena* arena)
{
	return (ArenaMarker)arena->arenaPointer;
}

void ArenaFreeMarker(Arena* arena, ArenaMarker marker)
{
	arena->arenaPointer = (void*)marker;
}


