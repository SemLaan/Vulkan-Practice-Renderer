#include "memory_debug_tools.h"

#ifndef GR_DIST

#include "allocators.h"
#include "containers/darray.h"
#include "containers/hashmap_u64.h"
#include "core/asserts.h"
#include "core/logger.h"
#include <stdlib.h>


#define ALLOCATIONS_MAP_SIZE 50000
#define ALLOCATIONS_MAP_MAX_COLLISIONS 1000


// Can turn a memory tag into a string, used for printing/logging
static const char* memTagToText[MAX_MEMORY_TAGS] = {
    "ALLOCATOR STATE    ",
    "SUB ARENA          ",
    "MEMORY SUBSYS      ",
    "LOGGING SUBSYS     ",
    "PLATFORM SUBSYS    ",
    "EVENT SUBSYS       ",
    "RENDERER SUBSYS    ",
    "INPUT SUBSYS       ",
    "GAME               ",
    "TEST               ",
    "DARRAY             ",
    "VERTEX BUFFER      ",
    "INDEX BUFFER       ",
    "TEXTURE            ",
    "HASHMAP            ",
    "MEMORY DEBUG       ",
};

// Allocator type to string, can be used for printing/logging
static const char* allocatorTypeToString[ALLOCATOR_TYPE_MAX_VALUE] = {
    "global",
    "freelist",
    "bump",
    "pool",
};

// Info about an allocation, this is stored for every allocation the game makes
typedef struct AllocInfo
{
    u32 allocatorId;        // Id of the allocator that owns this allocation
    const char* file;       // File where this allocation was created
    u32 line;               // Line where this allocation was created
    MemTag tag;             // Tag of this allocation
    u32 allocSize;          // Size of the alloction (ONLY the client size, state used by the allocator is not counted)
    u32 alignment;          // Alignment of the allocation
} AllocInfo;

DEFINE_DARRAY_TYPE_REF(AllocInfo);

// Info about a registered allocator, is stored for every allocator the game creates
typedef struct RegisteredAllocatorInfo
{
    const char* name;       // The allocators name
    Allocator* allocator;   // Pointer to the allocator
    u64 arenaStart;         // Start address of the allocator
    u64 arenaEnd;           // End address of the allocator
    u32 stateSize;          // Amount of memory the state of the allocator uses
    u32 allocatorId;        // Unique id of the allocator
    u32 parentAllocatorId;  // Id of the allocator that owns the allocator
    AllocatorType type;     // Type of allocator
} RegisteredAllocatorInfo;

DEFINE_DARRAY_TYPE(RegisteredAllocatorInfo);

// State
typedef struct MemoryDebugState
{
    u32 markedAllocatorId;                                  // ID of the marked allocator (there can only be one at a time)
    u64 arenaStart;                                         // Start of the allocator used by the debug tools
    u64 arenaEnd;                                           // End of the allocator used by the debug tools
    u64 arenaSize;                                          // Size of the arena used by the debug tools
    RegisteredAllocatorInfoDarray* registeredAllocatorDarray;// Darray for all the allocators that are going to be registered
    HashmapU64* allocationsMap;                             // Hashmap that maps all the allocations to their alloc info struct
    Allocator* allocInfoPool;                               // Pool allocator that contains all the alloc info structs
    u64 totalUserAllocated;                                 // Amount of memory allocated by the game
    u64 totalUserAllocationCount;                           // Amount of allocations done by the game
    u32 perTagAllocationCount[MAX_MEMORY_TAGS];             // Amount of allocations done per tag
    u32 perTagAllocated[MAX_MEMORY_TAGS];                   // Amount of memory allocated per tag
} MemoryDebugState;

static bool memoryDebuggingAllocatorsCreated = false;
static Allocator* memoryDebugAllocator;
static MemoryDebugState* state = nullptr;

// ========================================= startup and shutdown =============================================
void _StartMemoryDebugSubsys()
{
    const u64 memoryDebugArenaSize = MiB * 10;

    u64 memoryDebugArenaStart = 0;

    // Creating an allocator for the memory debug system to use
    if (!CreateGlobalAllocator("Debug allocator", memoryDebugArenaSize, &memoryDebugAllocator, nullptr, &memoryDebugArenaStart))
        GRASSERT_MSG(false, "Creating memory debug allocator failed");

    // Allocating and creating the memory debug state
    state = Alloc(memoryDebugAllocator, sizeof(*state), MEM_TAG_MEMORY_DEBUG);
    MemoryZero(state, sizeof(*state));
    state->allocationsMap = MapU64Create(memoryDebugAllocator, MEM_TAG_MEMORY_DEBUG, ALLOCATIONS_MAP_SIZE, ALLOCATIONS_MAP_MAX_COLLISIONS, Hash6432Shift);
    CreatePoolAllocator("Alloc info pool", memoryDebugAllocator, sizeof(AllocInfo), ALLOCATIONS_MAP_SIZE + ALLOCATIONS_MAP_MAX_COLLISIONS, &state->allocInfoPool);
    state->totalUserAllocated = 0;
    state->totalUserAllocationCount = 0;
    state->arenaStart = memoryDebugArenaStart;
    state->arenaSize = memoryDebugArenaSize;
    state->arenaEnd = memoryDebugArenaStart + memoryDebugArenaSize;
    state->registeredAllocatorDarray = RegisteredAllocatorInfoDarrayCreate(10, memoryDebugAllocator);
    state->markedAllocatorId = UINT32_MAX;

    memoryDebuggingAllocatorsCreated = true;
}

void _ShutdownMemoryDebugSubsys()
{
    // Let the OS clean all of this stuff up
}

// ============================================= Memory printing utils =============================================
// Takes in an amount of bytes and returns a string and an int indicating the order of magnitude of the amount of bytes
static const char* GetMemoryScaleString(u64 bytes, u64* out_scale)
{
    if (bytes < KiB)
    {
        *out_scale = 1;
        return "B";
    }
    else if (bytes < MiB)
    {
        *out_scale = KiB;
        return "KiB";
    }
    else if (bytes < GiB)
    {
        *out_scale = MiB;
        return "MiB";
    }
    else
    {
        *out_scale = GiB;
        return "GiB";
    }
}

// Prints a hierarchy of the registered allocators and information about them
// Call this on the game's global allocator and it will recurse through all its child allocators and print them out as well
static void PrintAllocatorStatsRecursively(RegisteredAllocatorInfo* root, u32 registeredAllocatorCount, u32 depth)
{
    // Creating a string with "depth" number of tabs
    char* tabs = Alloc(memoryDebugAllocator, depth + 1 /*null terminator*/, MEM_TAG_MEMORY_DEBUG);
    MemorySet(tabs, '\t', depth);
    tabs[depth] = 0;

    // printing info about the allocator
    _INFO("%s%s (id)%u, (type)%s", tabs, root->name, root->allocatorId, allocatorTypeToString[root->type]);

    // Calculating how much the arena is being used and printing that
    u64 arenaSize = root->arenaEnd - root->arenaStart;
    const char* scaleString;
    u64 scale;
    scaleString = GetMemoryScaleString(arenaSize, &scale);
    f32 arenaSizeScaled = (f32)arenaSize / (f32)scale;
    f32 usedAmount;

    switch (root->type)
    {
    case ALLOCATOR_TYPE_GLOBAL:
    case ALLOCATOR_TYPE_FREELIST:
        usedAmount = (f32)GetFreelistAllocatorArenaUsage(root->allocator);
        _INFO("%s%.2f/%.2f%s\t%.2f%%%% used", tabs, usedAmount / (f32)scale, arenaSizeScaled, scaleString, usedAmount / (f32)arenaSize * 100);
        break;
    case ALLOCATOR_TYPE_BUMP:
        usedAmount = (f32)GetBumpAllocatorArenaUsage(root->allocator);
        _INFO("%s%.2f/%.2f%s\t%.2f%%%% used", tabs, usedAmount / (f32)scale, arenaSizeScaled, scaleString, usedAmount / (f32)arenaSize * 100);
        break;
    case ALLOCATOR_TYPE_POOL:
        usedAmount = (f32)GetPoolAllocatorArenaUsage(root->allocator);
        _INFO("%s%.2f/%.2f%s\t%.2f%%%% used", tabs, usedAmount / (f32)scale, arenaSizeScaled, scaleString, usedAmount / (f32)arenaSize * 100);
        break;
    default:
        _ERROR("Unknown allocator type");
        break;
    }

    Free(memoryDebugAllocator, tabs);

    // Calling this function on all the current allocator's children
    for (u32 i = 0; i < registeredAllocatorCount; ++i)
    {
        if (root->allocatorId == state->registeredAllocatorDarray->data[i].parentAllocatorId)
            PrintAllocatorStatsRecursively(state->registeredAllocatorDarray->data + i, registeredAllocatorCount, depth + 1);
    }
}

void _PrintMemoryStats()
{
    _INFO("=======================================================================================================");
    _INFO("Printing memory stats:");

    // Printing allocators
    u32 registeredAllocatorCount = state->registeredAllocatorDarray->size;
    _INFO("Printing %u live allocators:", registeredAllocatorCount);

    RegisteredAllocatorInfo* globalAllocator = state->registeredAllocatorDarray->data;

    PrintAllocatorStatsRecursively(globalAllocator, registeredAllocatorCount, 1 /*start depth 1 to indent all allocators at least one tab*/);

    // Printing total allocation stats
    const char* scaleString;
    u64 scale;
    scaleString = GetMemoryScaleString(state->totalUserAllocated, &scale);
    _INFO("Total user allocation count: %llu", state->totalUserAllocationCount);
    _INFO("Total user allocated: %.2f%s", (f32)state->totalUserAllocated / (f32)scale, scaleString);

    // Printing allocation stats per tag
    _INFO("Allocations by tag:");
    for (u32 i = 0; i < MAX_MEMORY_TAGS; ++i)
    {
        _INFO("\t%s: %u", memTagToText[i], state->perTagAllocationCount[i]);
    }

    // Printing all active allocations
    // TODO: add a bool parameter to the function to specify whether to print this or not, because it's a lot
    AllocInfoRefDarray* allocInfoDarray = (AllocInfoRefDarray*)MapU64GetValueRefDarray(state->allocationsMap, memoryDebugAllocator);

    RegisteredAllocatorInfo* allocatedWithAllocator;

    _INFO("All active allocations:");
    for (u32 i = 0; i < allocInfoDarray->size; ++i)
    {
        AllocInfo* item = allocInfoDarray->data[i];

        allocatedWithAllocator = nullptr;

        for (u32 i = 0; i < registeredAllocatorCount; ++i)
        {
            if (state->registeredAllocatorDarray->data[i].allocatorId == item->allocatorId)
            {
                allocatedWithAllocator = state->registeredAllocatorDarray->data + i;
                break;
            }
        }

        if (allocatedWithAllocator == nullptr)
        {
            _FATAL("Live allocation with outdated allocator: %s:%u", item->file, item->line);
            GRASSERT_MSG(false, "Live allocation with outdated allocator");
        }
        else
            _INFO("\tAllocated by: (name)%s (id)%u (type)%s, Size: %u, File: %s:%u", allocatedWithAllocator->name, allocatedWithAllocator->allocatorId, allocatorTypeToString[allocatedWithAllocator->type], item->allocSize, item->file, item->line);
    }

    DarrayDestroy(allocInfoDarray);
    _INFO("=======================================================================================================");
}

void _MarkAllocator(Allocator* allocator)
{
    state->markedAllocatorId = allocator->id;
}

// ================================= Registering and unregistering allocators ====================================
static u32 nextAllocatorId = 0;

// Used by allocators to identify themselves to the memory debug tools
// This never returns zero, it just starts counting from one
static u32 _GetUniqueAllocatorId()
{
    nextAllocatorId++;
    return nextAllocatorId;
}

void _RegisterAllocator(u64 arenaStart, u64 arenaEnd, u32 stateSize, u32* out_allocatorId, AllocatorType type, Allocator* parentAllocator, const char* name, Allocator* allocator)
{
    // Making sure debug allocators don't get registered, we also don't have to worry about them getting unregistered as
    // they will be cleaned up by the OS, and thus don't call unregister
    // They get an id of zero which only debug allocators can get
    if (!memoryDebuggingAllocatorsCreated)
    {
        *out_allocatorId = 0;
        return;
    }

    *out_allocatorId = _GetUniqueAllocatorId();

    RegisteredAllocatorInfo allocatorInfo = {};
    allocatorInfo.name = name;
    allocatorInfo.allocator = allocator;
    allocatorInfo.allocatorId = *out_allocatorId;
    allocatorInfo.arenaStart = arenaStart;
    allocatorInfo.arenaEnd = arenaEnd;
    allocatorInfo.stateSize = stateSize;
    allocatorInfo.type = type;
    if (parentAllocator != nullptr)
        allocatorInfo.parentAllocatorId = parentAllocator->id;
    else
        allocatorInfo.parentAllocatorId = 0;

    DarrayPushback(state->registeredAllocatorDarray, &allocatorInfo);
}

void _UnregisterAllocator(u32 allocatorId, AllocatorType allocatorType)
{
    u32 registeredAllocatorCount = state->registeredAllocatorDarray->size;

    // Finding the allocator that is being unregistered in the array
    for (u32 i = 0; i < registeredAllocatorCount; ++i)
    {
        if (state->registeredAllocatorDarray->data[i].allocatorId == allocatorId)
        {
            // Removing all the info about the allocations that the allocator still contained
            u32 freedCount = _DebugFlushAllocator(state->registeredAllocatorDarray->data[i].allocator);
            if (freedCount > 0)
                _WARN("Destroyed allocator with %u active allocation(s)", freedCount);
            DarrayPopAt(state->registeredAllocatorDarray, i);
            return;
        }
    }

    _FATAL("Allocator with id: %u not found", allocatorId);
    _FATAL("Allocator type: %s", allocatorTypeToString[allocatorType]);
    GRASSERT_MSG(false, "Tried to destroy allocator that wasn't found");
}

// =========================================== Flushing an allocator =======================================================
// Returns the amount of allocations that were freed
u32 _DebugFlushAllocator(Allocator* allocator)
{
    // Getting an array of all allocations
    MapEntryU64RefDarray* mapEntriesDarray = MapU64GetMapEntryRefDarray(state->allocationsMap, memoryDebugAllocator);

    u32 freedAllocations = 0;

    // Looping through all allocations and clearing them if they are owned by the allocator being flushed
    for (u32 i = 0; i < mapEntriesDarray->size; ++i)
    {
        MapEntryU64* mapEntry = mapEntriesDarray->data[i];
        AllocInfo* allocInfo = mapEntry->value;

        if (allocator->id == allocInfo->allocatorId)
        {
            state->totalUserAllocationCount--;
            state->totalUserAllocated -= allocInfo->allocSize;
            state->perTagAllocated[allocInfo->tag] -= allocInfo->allocSize;
            state->perTagAllocationCount[allocInfo->tag]--;
            Free(state->allocInfoPool, allocInfo);
            MapU64Delete(state->allocationsMap, mapEntry->key);
            freedAllocations++;
        }
    }

    DarrayDestroy(mapEntriesDarray);

    return freedAllocations;
}

// ============================================= Debug alloc, realloc and free hook-ins =====================================
void* DebugAlignedAlloc(Allocator* allocator, u64 size, u32 alignment, MemTag memtag, const char* file, u32 line)
{
    // If debug allocation
    if (allocator->id == 0)
    {
        return allocator->BackendAlloc(allocator, size, alignment);
    }
    else // if normal allocation
    {
        // Updating total game allocation state
        state->totalUserAllocated += size;
        state->totalUserAllocationCount++;
        state->perTagAllocated[memtag] += size;
        state->perTagAllocationCount[memtag]++;

        // Letting the allocator handle the actual allocation
        void* allocation;

        if (allocator->id == state->markedAllocatorId)
            allocation = _aligned_malloc(size, alignment);
        else
            allocation = allocator->BackendAlloc(allocator, size, alignment);

        // Storing alloc info about the allocation
        AllocInfo* allocInfo = Alloc(state->allocInfoPool, sizeof(AllocInfo), MEM_TAG_MEMORY_DEBUG);
        allocInfo->allocatorId = allocator->id;
        allocInfo->alignment = alignment;
        allocInfo->allocSize = size;
        allocInfo->tag = memtag;
        allocInfo->file = file;
        allocInfo->line = line;
        MapU64Insert(state->allocationsMap, (u64)allocation, allocInfo);
        return allocation;
    }
}

void* DebugRealloc(Allocator* allocator, void* block, u64 newSize, const char* file, u32 line)
{
    // If debug allocation
    if (allocator->id == 0)
    {
        return allocator->BackendRealloc(allocator, block, newSize);
    }
    else // if normal allocation
    {
        // Deleting the old alloc info
        AllocInfo* oldAllocInfo = MapU64Delete(state->allocationsMap, (u64)block);

        // Checking if this allocation exists
        if (oldAllocInfo == nullptr)
        {
            _FATAL("Tried to realloc memory block that doesn't exists!, File: %s:%u", file, line);
            _FATAL("Address that was attempted to be reallocated: 0x%08x", (u64)block);
            GRASSERT(false);
        }

        // Checking if the realloc is using the wrong allocator
        if (oldAllocInfo->allocatorId != allocator->id)
        {
            _FATAL("Tried to realloc allocation with wrong allocator!");
            _FATAL("Allocation: %s:%u", oldAllocInfo->file, oldAllocInfo->line);
            _FATAL("Reallocation: %s:%u", file, line);
            u32 registeredAllocatorCount = state->registeredAllocatorDarray->size;
            const char* allocatorName;
            for (u32 i = 0; i < registeredAllocatorCount; ++i)
            {
                if (state->registeredAllocatorDarray->data[i].allocatorId == allocator->id)
                {
                    allocatorName = state->registeredAllocatorDarray->data[i].name;
                }
            }
            _FATAL("Wrong allocator: %s", allocatorName);
            for (u32 i = 0; i < registeredAllocatorCount; ++i)
            {
                if (state->registeredAllocatorDarray->data[i].allocatorId == oldAllocInfo->allocatorId)
                {
                    allocatorName = state->registeredAllocatorDarray->data[i].name;
                }
            }
            _FATAL("Correct allocator: %s", allocatorName);
            GRASSERT(false);
        }

        // Updating total game allocation info
        state->totalUserAllocated -= (oldAllocInfo->allocSize - newSize);
        state->perTagAllocated[oldAllocInfo->tag] -= (oldAllocInfo->allocSize - newSize);

        // Letting the allocator handle the actual reallocation
        void* reallocation;

        if (allocator->id == state->markedAllocatorId)
            reallocation = _aligned_realloc(block, newSize, oldAllocInfo->alignment);
        else
            reallocation = allocator->BackendRealloc(allocator, block, newSize);

        // Recreating a new alloc info for the changed allocation (the old alloc info gets deleted earlier in this function)
        AllocInfo* newAllocInfo = Alloc(state->allocInfoPool, sizeof(AllocInfo), MEM_TAG_MEMORY_DEBUG);
        newAllocInfo->allocatorId = oldAllocInfo->allocatorId;
        newAllocInfo->allocSize = newSize;
        newAllocInfo->tag = oldAllocInfo->tag;
        newAllocInfo->file = oldAllocInfo->file;
        newAllocInfo->line = oldAllocInfo->line;
        MapU64Insert(state->allocationsMap, (u64)reallocation, newAllocInfo);

        Free(state->allocInfoPool, oldAllocInfo);

        return reallocation;
    }
}

void DebugFree(Allocator* allocator, void* block, const char* file, u32 line)
{
    // If debug allocation
    if (allocator->id == 0)
    {
        allocator->BackendFree(allocator, block);
    }
    else // if normal allocation
    {
        // Deleting the alloc info
        AllocInfo* allocInfo = MapU64Delete(state->allocationsMap, (u64)block);
        if (allocInfo == nullptr)
        {
            _FATAL("Tried to free memory block that doesn't exists!, File: %s:%u", file, line);
            _FATAL("Address that was attempted to be freed: 0x%08x", (u64)block);
            GRASSERT(false);
        }

        // Checking if the free is using the wrong allocator
        if (allocInfo->allocatorId != allocator->id)
        {
            _FATAL("Tried to free allocation with wrong allocator!");
            _FATAL("Allocation: %s:%u", allocInfo->file, allocInfo->line);
            _FATAL("Free: %s:%u", file, line);
            u32 registeredAllocatorCount = state->registeredAllocatorDarray->size;
            const char* allocatorName;
            for (u32 i = 0; i < registeredAllocatorCount; ++i)
            {
                if (state->registeredAllocatorDarray->data[i].allocatorId == allocator->id)
                {
                    allocatorName = state->registeredAllocatorDarray->data[i].name;
                }
            }
            _FATAL("Wrong allocator: %s", allocatorName);
            for (u32 i = 0; i < registeredAllocatorCount; ++i)
            {
                if (state->registeredAllocatorDarray->data[i].allocatorId == allocInfo->allocatorId)
                {
                    allocatorName = state->registeredAllocatorDarray->data[i].name;
                }
            }
            _FATAL("Correct allocator: %s", allocatorName);
            GRASSERT(false);
        }

        // Updating total game memory info
        state->totalUserAllocationCount--;
        state->totalUserAllocated -= allocInfo->allocSize;
        state->perTagAllocated[allocInfo->tag] -= allocInfo->allocSize;
        state->perTagAllocationCount[allocInfo->tag]--;
        Free(state->allocInfoPool, allocInfo);

        // Letting the allocator do the actual freeing
        if (allocator->id == state->markedAllocatorId)
            _aligned_free(block);
        else
            allocator->BackendFree(allocator, block);
    }
}

#endif
