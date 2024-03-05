#pragma once

#include "defines.h"


typedef enum MemTag
{
	MEM_TAG_ALLOCATOR_STATE,
	MEM_TAG_SUB_ARENA,
	MEM_TAG_MEMORY_SUBSYS,
	MEM_TAG_LOGGING_SUBSYS,
	MEM_TAG_PLATFORM_SUBSYS,
	MEM_TAG_EVENT_SUBSYS,
	MEM_TAG_RENDERER_SUBSYS,
	MEM_TAG_INPUT_SUBSYS,
	MEM_TAG_GAME,
	MEM_TAG_TEST,
	MEM_TAG_DARRAY,
	MEM_TAG_VERTEX_BUFFER,
	MEM_TAG_INDEX_BUFFER,
	MEM_TAG_TEXTURE,
	MEM_TAG_HASHMAP,
    MEM_TAG_MEMORY_DEBUG,
	MAX_MEMORY_TAGS
} MemTag;

// Forward declaring allocator struct (see bottom of script for internals)
typedef struct Allocator Allocator;

// Pointer functions that abstract away allocation strategies
typedef void* (*PFN_BackendAlloc)(Allocator* allocator, u64 size, u32 alignment);
typedef void* (*PFN_BackendRealloc)(Allocator* allocator, void* block, u64 newSize);
typedef void (*PFN_BackendFree)(Allocator* allocator, void* block);

// Enum of allocator types
typedef enum AllocatorType
{
	ALLOCATOR_TYPE_GLOBAL,
	ALLOCATOR_TYPE_FREELIST,
	ALLOCATOR_TYPE_BUMP,
	ALLOCATOR_TYPE_POOL,
	ALLOCATOR_TYPE_MAX_VALUE,
} AllocatorType;


// The client of this struct should not touch it's internals
typedef struct Allocator
{
	PFN_BackendAlloc BackendAlloc;				// Function that handles allocation
	PFN_BackendRealloc BackendRealloc;			// Function that handles reallocation
	PFN_BackendFree BackendFree;				// Function that handles freeing
	void* backendState;							// Hidden state that may be specific to allocator types
    Allocator* parentAllocator;					// The allocator that was used to allocate the memory for this allocator
	u32 id; 									// Is only usefull in debug mode, is always zero in dist mode (Used by memory debug tools)
} Allocator;