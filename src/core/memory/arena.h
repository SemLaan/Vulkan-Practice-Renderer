#pragma once
#include "defines.h"
#include "../meminc.h"


typedef struct Arena
{
	void* memoryBlock;
	void* arenaPointer;
	size_t arenaCapacity;
} Arena;

Arena ArenaCreate(Allocator* allocator, size_t size);
void ArenaDestroy(Arena* arena, Allocator* allocator);

void* ArenaAlloc(Arena* arena, size_t allocSize);
void* ArenaAlignedAlloc(Arena* arena, size_t allocSize, size_t allocAlignment);
void ArenaClear(Arena* arena);

