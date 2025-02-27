#include "memory_subsys.h"

#include "../logger.h"
#include "../asserts.h"
#include <string.h>



typedef struct MemoryState
{
	Allocator* globalAllocator;	// Global freelist allocator that owns all the game's memory
	size_t arenaSize;			// Arena size of the global allocator, and thus of the game
} MemoryState;

static MemoryState* state = nullptr;
static bool initialized = false;


bool InitializeMemory(size_t requiredMemory)
{
	GRASSERT_DEBUG(state == nullptr); // If this fails it means init was called twice
	_INFO("Initializing memory subsystem...");
	initialized = false;

	// Creating the global allocator and allocating all application memory
	Allocator* globalAllocator;
	size_t globalAllocatorStateSize;
	if (!CreateGlobalAllocator("Global allocator", requiredMemory, &globalAllocator, &globalAllocatorStateSize, nullptr))
	{
		_FATAL("Creating global allocator failed");
		return false;
	}

	// Creating the memory state
	state = Alloc(globalAllocator, sizeof(MemoryState));
	initialized = true;

	state->globalAllocator = globalAllocator;
	state->arenaSize = requiredMemory + globalAllocatorStateSize;

	return true;
}

void ShutdownMemory()
{
	if (state == nullptr)
	{
		_INFO("Memory startup failed, skipping shutdown");
		return;
	}
	else
	{
		_INFO("Shutting down memory subsystem...");
	}

	initialized = false;

	Allocator* globalAllocator = state->globalAllocator;
	Free(GetGlobalAllocator(), state);

	PRINT_MEMORY_STATS();

	// Return all the application memory back to the OS
	DestroyGlobalAllocator(globalAllocator);
}

Allocator* GetGlobalAllocator()
{
	return state->globalAllocator;
}

