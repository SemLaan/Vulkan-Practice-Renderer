#pragma once
#include "defines.h"
#include "allocators.h"

// Creates the global allocator, thats pretty much it
bool InitializeMemory(size_t requiredMemory);

void ShutdownMemory();

Allocator* GetGlobalAllocator();
