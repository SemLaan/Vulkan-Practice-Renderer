#pragma once
#include "defines.h"

#include "allocator_types.h"
#include "memory_debug_tools.h"


#ifdef DIST

// void* Alloc(Allocator* allocator, u64 size, MemTag memtag);
#define Alloc(allocator, size) (allocator)->BackendAlloc((allocator), (size), MIN_ALIGNMENT)
// void* AlignedAlloc(Allocator* allocator, u64 size, u32 alignment, MemTag memtag);
#define AlignedAlloc(allocator, size, alignment) (allocator)->BackendAlloc((allocator), (size), (alignment))
// void* Realloc(Allocator* allocator, void* block, u64 newSize);
#define Realloc(allocator, block, newSize) (allocator)->BackendRealloc((allocator), (block), (newSize))
// void Free(Allocator* allocator, void* block);
#define Free(allocator, block) (allocator)->BackendFree((allocator), (block))

#else // if not dist

// void* Alloc(Allocator* allocator, u64 size, MemTag memtag);
#define Alloc(allocator, size) DebugAlignedAlloc((allocator), (size), MIN_ALIGNMENT, __FILE__, __LINE__)
// void* AlignedAlloc(Allocator* allocator, u64 size, u32 alignment, MemTag memtag);
#define AlignedAlloc(allocator, size, alignment) DebugAlignedAlloc((allocator), (size), (alignment), __FILE__, __LINE__)
// void* Realloc(Allocator* allocator, void* block, u64 newSize);
#define Realloc(allocator, block, newSize) DebugRealloc((allocator), (block), (newSize), __FILE__, __LINE__)
// void Free(Allocator* allocator, void* block);
#define Free(allocator, block) DebugFree((allocator), (block), __FILE__, __LINE__)

#endif

// ================================== Global allocators (not an allocator type) =================================================================================================================================================
// These allocators call malloc instead of getting their memory from a parent allocator, but they're just freelist allocators
bool CreateGlobalAllocator(const char* name, size_t arenaSize, Allocator** out_allocator, size_t* out_stateSize, u64* out_arenaStart);
void DestroyGlobalAllocator(Allocator* allocator);

// ================================== Freelist allocator =================================================================================================================================================
// Creates a freelist allocator with the given name, uses parentAllocator to allocate this allocators memory, has arenaSize bytes
// out_allocator is expected to be a pointer to the pointer to the allocator struct
void CreateFreelistAllocator(const char* name, Allocator* parentAllocator, size_t arenaSize, Allocator** out_allocator);
void DestroyFreelistAllocator(Allocator* allocator);
// Returns the amount of free nodes (fairly useless function tbh)
size_t FreelistGetFreeNodes(void* backendState);
// Returns the size of the freelist allocation headers in bytes
u32 GetFreelistAllocHeaderSize();
// Returns how many bytes of this allocator are allocated // TODO: figure out if this includes headers or only memory available to the client
u64 GetFreelistAllocatorArenaUsage(Allocator* allocator);

// ==================================== Bump allocator ================================================================================================================================================
// Creates a bump (aka linear or scratch) allocator with the given name, uses parentAllocator to allocate this allocators memory, has arenaSize bytes
// out_allocator is expected to be a pointer to the pointer to the allocator struct
void CreateBumpAllocator(const char* name, Allocator* parentAllocator, size_t arenaSize, Allocator** out_allocator);
void DestroyBumpAllocator(Allocator* allocator);
// Returns how many bytes of this allocator are allocated // TODO: figure out if this includes headers or only memory available to the client
u64 GetBumpAllocatorArenaUsage(Allocator* allocator);

// ===================================== Pool allocator =============================================================================================================================================
// Size of blocks this allocator returns, and amount of blocks in this allocator.
// all blocks created by this allocator are aligned on allocSize (provided it is a power of two)
void CreatePoolAllocator(const char* name, Allocator* parentAllocator, u32 blockSize, u32 poolSize, Allocator** out_allocator);
void DestroyPoolAllocator(Allocator* allocator);
// Clears all allocations from this allocator, WARNING: this thus invalidates all pointers handed out by this allocator, this can be very fast but also dangerous, only use if you know what you're doing
void FlushPoolAllocator(Allocator* allocator);
// Returns how many bytes of this allocator are allocated // TODO: figure out if this includes headers or only memory available to the client
u64 GetPoolAllocatorArenaUsage(Allocator* allocator);
