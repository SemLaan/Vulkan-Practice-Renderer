#include "allocators.h"

#include <core/asserts.h>
// TODO: remove this, since im pretty sure it's only used for ceil and floor
#include <math.h>
// This is here for malloc, this is the only place it's called
#include <stdlib.h>

#include "mem_utils.h"


// =====================================================================================================================================================================================================
// ================================== Freelist allocator ===============================================================================================================================================
// =====================================================================================================================================================================================================
#define FREELIST_NODE_FACTOR 10

// Freelist allocator stores this in front of the user block to keep track of allocation size and alignment
typedef struct FreelistAllocHeader
{
    void* start;            // Start of the actual allocation, not the client address       
    u32 size;               // Size of the client allocation
    u32 alignment;          // Alignment of the client allocation
    // End should be 4 byte aligned
} FreelistAllocHeader;

typedef struct FreelistNode
{
    void* address;              // Address of this free block
    size_t size;                // Size of this free block
    struct FreelistNode* next;  // Pointer to the freelist node after this (i'm fairly certain the list is ordered based on address but not 100% sure)
} FreelistNode;

// End should be 4 byte aligned
typedef struct FreelistState
{
    void* arenaStart;           // Arena start address
    size_t arenaSize;           // Arena size
    FreelistNode* head;         // The first free node in the arena
    FreelistNode* nodePool;     // Address of the array that is used for the freelist node pool
    u32 nodeCount;              // Amount of free nodes the allocator can have
} FreelistState;

// These functions use other functions to do allocations and prepare the blocks for use
// (adding a header and aligning the block)
static void* FreelistAlignedAlloc(Allocator* allocator, u64 size, u32 alignment);
static void* FreelistReAlloc(Allocator* allocator, void* block, u64 size);
static void FreelistFree(Allocator* allocator, void* block);

// These functions do actual allocation and freeing
static void* FreelistPrimitiveAlloc(void* backendState, size_t size);
static bool FreelistPrimitiveTryReAlloc(void* backendState, void* block, size_t oldSize, size_t newSize);
static void FreelistPrimitiveFree(void* backendState, void* block, size_t size);

void CreateFreelistAllocator(const char* name, Allocator* parentAllocator, size_t arenaSize, Allocator** out_allocator, bool muteDestruction)
{
    // Calculating the required nodes for an arena of the given size
    // Make one node for every "freelist node factor" nodes that fit in the arena
    u32 nodeCount = (u32)(arenaSize / (FREELIST_NODE_FACTOR * sizeof(FreelistNode))); // TODO: make this logarithmic instead of linear, because big allocators make way too much space for nodes

    // Calculating required memory (client size + state size)
    size_t stateSize = sizeof(FreelistState) + nodeCount * sizeof(FreelistNode);
    size_t requiredMemory = arenaSize + stateSize;

    // Allocating memory for state and arena and zeroing state memory
    void* arenaBlock = Alloc(parentAllocator, requiredMemory);
    MemoryZero(arenaBlock, stateSize);

    // Getting pointers to the internal components of the allocator
    FreelistState* state = (FreelistState*)arenaBlock;
    FreelistNode* nodePool = (FreelistNode*)((u8*)arenaBlock + sizeof(FreelistState)); // Alignment should be fine, the end of FreelistState is at least 4 byte aligned
    void* arenaStart = (u8*)arenaBlock + stateSize;

    // Configuring allocator state
    state->arenaStart = arenaStart;
    state->arenaSize = arenaSize;
    state->head = nodePool;
    state->nodePool = nodePool;
    state->nodeCount = nodeCount;

    // Configuring head node
    state->head->address = arenaStart;
    state->head->size = arenaSize;
    state->head->next = nullptr;

    Allocator* allocator = Alloc(parentAllocator, sizeof(*allocator));

    // Linking the allocator object to the freelist functions
    allocator->BackendAlloc = FreelistAlignedAlloc;
    allocator->BackendRealloc = FreelistReAlloc;
    allocator->BackendFree = FreelistFree;
    allocator->backendState = state;
    allocator->parentAllocator = parentAllocator;

    *out_allocator = allocator;

    REGISTER_ALLOCATOR((u64)arenaStart, (u64)arenaStart + arenaSize, stateSize, &allocator->id, ALLOCATOR_TYPE_FREELIST, parentAllocator, name, allocator, muteDestruction);
}

void DestroyFreelistAllocator(Allocator* allocator)
{
    FreelistState* state = (FreelistState*)allocator->backendState;

    UNREGISTER_ALLOCATOR(allocator->id, ALLOCATOR_TYPE_FREELIST);

    // Frees the entire arena including state
    Free(allocator->parentAllocator, state);
    Free(allocator->parentAllocator, allocator);
}

size_t FreelistGetFreeNodes(void* backendState)
{
    FreelistState* state = (FreelistState*)backendState;
    size_t count = 0;
    FreelistNode* node = state->head;

    while (node)
    {
        count++;
        node = node->next;
    }

    return count;
}

u32 GetFreelistAllocHeaderSize()
{
    return sizeof(FreelistAllocHeader);
}

u64 GetFreelistAllocatorArenaUsage(Allocator* allocator)
{
    FreelistState* state = (FreelistState*)allocator->backendState;
    u64 freeAmount = 0;
    FreelistNode* node = state->head;

    while (node)
    {
        freeAmount += node->size;
        node = node->next;
    }

    return state->arenaSize - freeAmount;
}

static FreelistNode* GetNodeFromPool(FreelistState* state)
{
    for (u32 i = 0; i < state->nodeCount; ++i)
    {
        if (state->nodePool[i].address == nullptr)
            return state->nodePool + i;
    }

    GRASSERT_MSG(false, "Ran out of pool nodes, decrease the x variable in the freelist allocator: get required memory size function. Or even better make more use of local allocators to avoid fragmentation");
    return nullptr;
}

static void ReturnNodeToPool(FreelistNode* node)
{
    node->address = nullptr;
    node->next = nullptr;
    node->size = 0;
}

static void* FreelistAlignedAlloc(Allocator* allocator, u64 size, u32 alignment)
{
	// Checking if the alignment is greater than min alignment and is a power of two
    GRASSERT_DEBUG((alignment >= MIN_ALIGNMENT) && ((alignment & (alignment - 1)) == 0));

    u32 requiredSize = (u32)size + sizeof(FreelistAllocHeader) + alignment - 1;

    void* block = FreelistPrimitiveAlloc(allocator->backendState, requiredSize);
    u64 blockExcludingHeader = (u64)block + sizeof(FreelistAllocHeader);
    // Gets the next address that is aligned on the requested boundary
    void* alignedBlock = (void*)((blockExcludingHeader + alignment - 1) & ~((u64)alignment - 1));

    // Putting in the header
    FreelistAllocHeader* header = (FreelistAllocHeader*)alignedBlock - 1;
    header->start = block;
    header->size = (u32)size;
    header->alignment = alignment;

    // return the block to the client
    return alignedBlock;
}

static void* FreelistReAlloc(Allocator* allocator, void* block, u64 size)
{
// Going slightly before the block and grabbing the alloc header that is stored there for debug info
    FreelistAllocHeader* header = (FreelistAllocHeader*)block - 1;
    GRASSERT(size != header->size);
    u64 newTotalSize = size + header->alignment - 1 + sizeof(FreelistAllocHeader);
    u64 oldTotalSize = header->size + header->alignment - 1 + sizeof(FreelistAllocHeader);

    // ================== If the realloc is smaller than the original alloc ==========================
    // ===================== Or if there is enough space after the old alloc to just extend it ========================
    if (FreelistPrimitiveTryReAlloc(allocator->backendState, header->start, oldTotalSize, newTotalSize))
    {
        header->size = (u32)size;
        return block;
    }

    // ==================== If there's no space at the old allocation ==========================================
    // ======================= Copy it to a new allocation and delete the old one ===============================
    // Get new allocation and align it
    void* newBlock = FreelistPrimitiveAlloc(allocator->backendState, newTotalSize);
    u64 blockExcludingHeader = (u64)newBlock + sizeof(FreelistAllocHeader);
    void* alignedBlock = (void*)((blockExcludingHeader + header->alignment - 1) & ~((u64)header->alignment - 1));

    // Copy the client data
    MemoryCopy(alignedBlock, block, header->size);

    // Fill in the header at the new memory location
    FreelistAllocHeader* newHeader = (FreelistAllocHeader*)alignedBlock - 1;
    newHeader->start = newBlock;
    newHeader->size = (u32)size;
    newHeader->alignment = header->alignment;

    // Free the old data
    FreelistPrimitiveFree(allocator->backendState, header->start, oldTotalSize);

    return alignedBlock;
}

static void FreelistFree(Allocator* allocator, void* block)
{
	// Going slightly before the block and grabbing the alloc header that is stored there for debug info
    FreelistAllocHeader* header = (FreelistAllocHeader*)block - 1;
    u64 totalFreeSize = header->size + header->alignment - 1 + sizeof(FreelistAllocHeader);

    FreelistPrimitiveFree(allocator->backendState, header->start, totalFreeSize);
}

// Allocates a block without worrying about remembering that block (doesn't store a header)
static void* FreelistPrimitiveAlloc(void* backendState, size_t size)
{
	FreelistState* state = (FreelistState*)backendState;
	FreelistNode* node = state->head;
	FreelistNode* previous = nullptr;

	while (node)
	{
		// If this node is the exact required size just use it
		if (node->size == size)
		{
			// Preparing the block to return to the client
			void* block = node->address;
			// Removing the node from the list and linking the list back together
			if (previous)
				previous->next = node->next;
			else // If the node is the head
				state->head = node->next;
			ReturnNodeToPool(node);
			return block;
		}
		// If this node is greater in size than requested, use it and split the node
		else if (node->size > size)
		{
			// Preparing the block to return to the client
			void* block = node->address;
			// Removing the now allocated memory from the node
			node->size -= size;
			node->address = (u8*)node->address + size;
			return block;
		}

		// If this node is smaller than the requested size, go to next node
		previous = node;
		node = node->next;
	}

	_FATAL("Can't allocate object of size %llu", size);
	GRASSERT_MSG(false, "Freelist allocator ran out of memory or too fragmented");
	return nullptr;
}

// Tries to reallocate a block without worrying about remembering that block (doesn't store a header)
static bool FreelistPrimitiveTryReAlloc(void* backendState, void* block, size_t oldSize, size_t newSize)
{
	FreelistState* state = (FreelistState*)backendState;

	// ====== If the requested size is smaller than the allocated size just return block and free the leftovers of the block ==========================
	if (oldSize > newSize)
	{
		u32 freedSize = (u32)oldSize - (u32)newSize;
		FreelistPrimitiveFree(backendState, (u8*)block + oldSize, freedSize); // Freeing the memory
		return true;
	}
	else
	{
		// =================== If the requested size is bigger than our allocation ==============================
		// Trying to find a free node next in line that we can add to our allocation
		u32 requiredNodeSize = (u32)newSize - (u32)oldSize;
		void* requiredAddress = (u8*)block + oldSize;

		FreelistNode* node = state->head;
		FreelistNode* previous = nullptr;

		while (node)
		{
			// ==== If we find a free node right at the end of our allocation ================================
			if (node->address == requiredAddress)
			{
				if (node->size < requiredNodeSize)
					return false;
				// This if else statement updates the freelist
				if (node->size == requiredNodeSize)
				{
					if (previous)
						previous->next = node->next;
					else // If the node is the head
						state->head = node->next;
					ReturnNodeToPool(node);
				}
				else // If the node is not the exact required size
				{
					node->address = (u8*)node->address + requiredNodeSize;
					node->size -= requiredNodeSize;
				}
				return true;
			}
			else if (node->address > requiredAddress)
				return false;
			// If the block being reallocated sits after the current node, go to the next node
			previous = node;
			node = node->next;
		}
	}
	return false;
}

// Frees a block
static void FreelistPrimitiveFree(void* backendState, void* block, size_t size)
{
	FreelistState* state = (FreelistState*)backendState;

	if (!state->head)
	{
		state->head = GetNodeFromPool(state);
		state->head->address = block;
		state->head->size = size;
		state->head->next = nullptr;
		return;
	}

	FreelistNode* node = state->head;
	FreelistNode* previous = nullptr;

	while (node || previous)
	{
		// If freed block sits before the current free node, or we're at the end of the list
		if ((node == nullptr) ? true : node->address > block)
		{
			// True if previous exists and end of previous aligns with start of freed block
			u8 aligns = previous ? ((u8*)previous->address + previous->size) == block : false;
			// True if the end of the freed block aligns with the start of the next node (also checks if node exist in case we are at the end of the list)
			aligns |= node ? (((u8*)block + size) == node->address) << 1 : false;

			// aligns:
			// 00 if nothing aligns
			// 01 if the previous aligns
			// 10 if the next aligns
			// 11 if both align

			FreelistNode* newNode = nullptr;

			switch (aligns)
			{
			case 0b00: // Nothing aligns ====================
				newNode = GetNodeFromPool(state);
				newNode->next = node;
				newNode->address = block;
				newNode->size = size;
				if (previous)
					previous->next = newNode;
				else
					state->head = newNode;
				return;
			case 0b01: // Previous aligns ===================
				previous->size += size;
				return;
			case 0b10: // Next aligns =======================
				node->address = block;
				node->size += size;
				return;
			case 0b11: // Previous and next align ===========
				previous->next = node->next;
				previous->size += size + node->size;
				ReturnNodeToPool(node);
				return;
			}
		}

		// If the block being freed sits after the current node, go to the next node
		previous = node;
		node = node->next;
	}

	GRASSERT_MSG(false, "I have no idea what went wrong, somehow the freelist free operation failed, good luck :)");
}

// =====================================================================================================================================================================================================
// ================================== Bump allocator ===================================================================================================================================================
// =====================================================================================================================================================================================================
typedef struct BumpAllocatorState
{
    void* arenaStart;       // Start of the arena
    void* bumpPointer;      // Points to the next free address
    size_t arenaSize;       // Size of the arena
    u32 allocCount;         // Amount of active allocations
} BumpAllocatorState;

// These functions do allocation and alignment
static void* BumpAlignedAlloc(Allocator* allocator, u64 size, u32 alignment);
static void* BumpReAlloc(Allocator* allocator, void* block, u64 size);
static void BumpFree(Allocator* allocator, void* block);


void CreateBumpAllocator(const char* name, Allocator* parentAllocator, size_t arenaSize, Allocator** out_allocator, bool muteDestruction)
{
    // Calculating required memory (client size + state size)
    size_t stateSize = sizeof(BumpAllocatorState);
    size_t requiredMemory = arenaSize + stateSize;

    // Allocating memory for state and arena and zeroing state memory
    void* arenaBlock = Alloc(parentAllocator, requiredMemory);
    MemoryZero(arenaBlock, stateSize);

    // Getting pointers to the internal components of the allocator
    BumpAllocatorState* state = (BumpAllocatorState*)arenaBlock;
    void* arenaStart = (u8*)arenaBlock + stateSize;

    // Configuring allocator state
    state->arenaStart = arenaStart;
    state->arenaSize = arenaSize;
    state->bumpPointer = arenaStart;
    state->allocCount = 0;

    Allocator* allocator = Alloc(parentAllocator, sizeof(*allocator));

    // Linking the allocator object to the freelist functions
    allocator->BackendAlloc = BumpAlignedAlloc;
    allocator->BackendRealloc = BumpReAlloc;
    allocator->BackendFree = BumpFree;
    allocator->backendState = state;
    allocator->parentAllocator = parentAllocator;

    *out_allocator = allocator;

    REGISTER_ALLOCATOR((u64)arenaStart, (u64)arenaStart + arenaSize, stateSize, &allocator->id, ALLOCATOR_TYPE_BUMP, parentAllocator, name, allocator, muteDestruction);
}

void DestroyBumpAllocator(Allocator* allocator)
{
    BumpAllocatorState* state = (BumpAllocatorState*)allocator->backendState;

    UNREGISTER_ALLOCATOR(allocator->id, ALLOCATOR_TYPE_BUMP);

    // Frees the entire arena including state
    Free(allocator->parentAllocator, state);

    Free(allocator->parentAllocator, allocator);
}

u64 GetBumpAllocatorArenaUsage(Allocator* allocator)
{
    BumpAllocatorState* state = (BumpAllocatorState*)allocator->backendState;

    return (u64)state->bumpPointer - (u64)state->arenaStart;
}

static void* BumpAlignedAlloc(Allocator* allocator, u64 size, u32 alignment)
{
    BumpAllocatorState* state = (BumpAllocatorState*)allocator->backendState;

	// Checking if the alignment is greater than min alignment and is a power of two
    GRASSERT_DEBUG((alignment >= MIN_ALIGNMENT) && ((alignment & (alignment - 1)) == 0));

    u32 requiredSize = (u32)size + alignment - 1;

    // Allocating the actual block
    void* block = state->bumpPointer;
    state->bumpPointer = (u8*)state->bumpPointer + requiredSize;
    state->allocCount++;
    GRASSERT_MSG((u8*)state->bumpPointer <= ((u8*)state->arenaStart + state->arenaSize), "Bump allocator overallocated");

    // Gets the next address that is aligned on the requested boundary
    void* alignedBlock = (void*)(((u64)block + alignment - 1) & ~((u64)alignment - 1));

    // return the block to the client
    return alignedBlock;
}

static void* BumpReAlloc(Allocator* allocator, void* block, u64 size)
{
	GRASSERT_MSG(false, "Reallocating with bump allocator not allowed");

    return nullptr;
}

static void BumpFree(Allocator* allocator, void* block)
{
    BumpAllocatorState* state = (BumpAllocatorState*)allocator->backendState;

    state->allocCount--;

    if (state->allocCount == 0)
    {
        state->bumpPointer = state->arenaStart;
    }
}

static void* BumpPrimitiveAlloc(void* backendState, size_t size)
{
    BumpAllocatorState* state = (BumpAllocatorState*)backendState;

    // Allocating the actual block
    void* block = state->bumpPointer;
    state->bumpPointer = (u8*)state->bumpPointer + size;
    state->allocCount++;
    GRASSERT_MSG((u8*)state->bumpPointer <= ((u8*)state->arenaStart + state->arenaSize), "Bump allocator overallocated");
    return block;
}

// =====================================================================================================================================================================================================
// ===================================== Pool allocator =============================================================================================================================================
// =====================================================================================================================================================================================================
typedef struct PoolAllocatorState
{
    void* poolStart;		// Pointer to the start of the memory that is managed by this allocator
    u32* controlBlocks;		// Pointer to the bitblocks that keep track of which blocks are free and which aren't
    u32 blockSize;			// Size of each block
    u32 poolSize;			// Amount of blocks in the pool
	u32 controlBlockCount;	// Amount of bitblocks in controlBlocks (each bitblock is a u32 that kan keep track of 32 blocks in the pool)
} PoolAllocatorState;

static void* PoolAlignedAlloc(Allocator* allocator, u64 size, u32 alignment);
static void* PoolReAlloc(Allocator* allocator, void* block, u64 size);
static void PoolFree(Allocator* allocator, void* block);

void CreatePoolAllocator(const char* name, Allocator* parentAllocator, u32 blockSize, u32 poolSize, Allocator** out_allocator, bool muteDestruction)
{
    // Calculating required memory (client size + state size)
    u32 stateSize = sizeof(PoolAllocatorState);
    u32 blockTrackerSize = 4 * ceil((f32)poolSize / 32.f);
    u32 arenaSize = (blockSize * poolSize) + /*for alignment purposes*/ (blockSize - 1);
    u32 requiredMemory = arenaSize + stateSize + blockTrackerSize;

    // Allocating memory for state and arena and zeroing state memory
    void* arenaBlock = Alloc(parentAllocator, requiredMemory);
    MemoryZero(arenaBlock, stateSize + blockTrackerSize);

    // Getting pointers to the internal components of the allocator
    PoolAllocatorState* state = (PoolAllocatorState*)arenaBlock;
    state->controlBlocks = (u32*)((u8*)arenaBlock + stateSize);
    state->poolStart = (void*)((u64)(((u8*)state->controlBlocks + blockTrackerSize) + blockSize - 1) & ~((u64)blockSize - 1));

    // Configuring allocator state
    state->blockSize = blockSize;
    state->poolSize = poolSize;
	state->controlBlockCount = ceil((f32)poolSize / 32.f);

    Allocator* allocator = Alloc(parentAllocator, sizeof(*allocator));

    // Linking the allocator object to the freelist functions
    allocator->BackendAlloc = PoolAlignedAlloc;
    allocator->BackendRealloc = PoolReAlloc;
    allocator->BackendFree = PoolFree;
    allocator->backendState = state;
    allocator->parentAllocator = parentAllocator;

    *out_allocator = allocator;

    REGISTER_ALLOCATOR((u64)state->poolStart, (u64)state->poolStart + (blockSize * poolSize), stateSize + blockTrackerSize, &allocator->id, ALLOCATOR_TYPE_POOL, parentAllocator, name, allocator, muteDestruction);
}

void DestroyPoolAllocator(Allocator* allocator)
{
    PoolAllocatorState* state = (PoolAllocatorState*)allocator->backendState;

    UNREGISTER_ALLOCATOR(allocator->id, ALLOCATOR_TYPE_POOL);

    // Frees the entire arena including state
    Free(allocator->parentAllocator, state);
    Free(allocator->parentAllocator, allocator);
}

void FlushPoolAllocator(Allocator* allocator)
{
    PoolAllocatorState* state = (PoolAllocatorState*)allocator->backendState;

    DEBUG_FLUSH_ALLOCATOR(allocator);

    for (u32 i = 0; i < state->controlBlockCount; ++i)
	{
		state->controlBlocks[i] = 0;
	}
}

// From: http://tekpool.wordpress.com/category/bit-count/
u32 BitCount(u32 u)
{
	u32 uCount;

	uCount = u - ((u >> 1) & 033333333333) - ((u >> 2) & 011111111111);
	
	return ((uCount + (uCount >> 3)) & 030707070707) % 63;
}

// This is used for finding the first free block in the pool quickly
u32 First0Bit(u32 i)
{
	i = ~i;
	return BitCount((i & (-i)) - 1);
}

u64 GetPoolAllocatorArenaUsage(Allocator* allocator)
{
    PoolAllocatorState* state = (PoolAllocatorState*)allocator->backendState;

    u32 takenBlocks = 0;

    for (u32 i = 0; i < state->controlBlockCount; ++i)
	{
		takenBlocks += BitCount(state->controlBlocks[i]);
	}

    return state->blockSize * takenBlocks;
}

static void* PoolAlignedAlloc(Allocator* allocator, u64 size, u32 alignment)
{
	PoolAllocatorState* state = (PoolAllocatorState*)allocator->backendState;

	GRASSERT_DEBUG(alignment == MIN_ALIGNMENT);
	GRASSERT_DEBUG(size <= state->blockSize);

	u32 firstFreeBlock = UINT32_MAX;

	for (u32 i = 0; i < state->controlBlockCount; ++i)
	{
		if (state->controlBlocks[i] == UINT32_MAX)
			continue;
		
		u32 firstZeroBit = First0Bit(state->controlBlocks[i]);
		firstFreeBlock = (i * 32/*amount of bits in 32 bit int*/) + firstZeroBit;
		state->controlBlocks[i] |= 1 << firstZeroBit;
		break;
	}

	GRASSERT_MSG(firstFreeBlock < state->poolSize, "Pool allocator ran out of blocks");

	return (u8*)state->poolStart + (state->blockSize * firstFreeBlock);
}

static void* PoolReAlloc(Allocator* allocator, void* block, u64 size)
{
	GRASSERT_MSG(false, "Error, shit programmer detected!");
	return nullptr;
}

static void PoolFree(Allocator* allocator, void* block)
{
	PoolAllocatorState* state = (PoolAllocatorState*)allocator->backendState;

	u64 blockAddress = (u64)block;
	u64 poolStartAddress = (u64)state->poolStart;

	u64 relativeAddress = blockAddress - poolStartAddress;
	u64 poolBlockAddress = relativeAddress / state->blockSize;

	u32 controlBlockIndex = floor((f32)poolBlockAddress / 32.f);
	u32 bitAddress = poolBlockAddress % 32;

	// Setting the bit that manages the freed block to zero
	// Inverting the bits in the bitblock, then setting the bit to one, then inverting the block again
	state->controlBlocks[controlBlockIndex] = ~((1 << bitAddress) | (~state->controlBlocks[controlBlockIndex]));
}

// =====================================================================================================================================================================================================
// ================================== Global allocator creation =====================================================================
// =====================================================================================================================================================================================================
bool CreateGlobalAllocator(const char* name, size_t arenaSize, Allocator** out_allocator, size_t* out_stateSize, u64* out_arenaStart)
{
    // Calculating the required nodes for an arena of the given size
    // Make one node for every "freelist node factor" nodes that fit in the arena
    u32 nodeCount = (u32)(arenaSize / (FREELIST_NODE_FACTOR * sizeof(FreelistNode)));

    // Calculating required memory (client size + state size)
    size_t stateSize = sizeof(FreelistState) + nodeCount * sizeof(FreelistNode);
    if (out_stateSize)
        *out_stateSize = stateSize;
    size_t requiredMemory = arenaSize + stateSize;

    // Allocating memory for state and arena and zeroing state memory
    void* arenaBlock = malloc(requiredMemory);
    if (arenaBlock == nullptr)
    {
        _FATAL("Couldn't allocate arena memory, tried allocating %lluB, initializing memory failed", requiredMemory);
        return false;
    }

    MemoryZero(arenaBlock, stateSize);

    // Getting pointers to the internal components of the allocator
    FreelistState* state = (FreelistState*)arenaBlock;
    FreelistNode* nodePool = (FreelistNode*)((u8*)arenaBlock + sizeof(FreelistState));
    void* arenaStart = (u8*)arenaBlock + stateSize;

    if (out_arenaStart)
        *out_arenaStart = (u64)arenaStart;

    // Configuring allocator state
    state->arenaStart = arenaStart;
    state->arenaSize = arenaSize;
    state->head = nodePool;
    state->nodePool = nodePool;
    state->nodeCount = nodeCount;

    // Configuring head node
    state->head->address = arenaStart;
    state->head->size = arenaSize;
    state->head->next = nullptr;

    Allocator* allocator = malloc(sizeof(*allocator));

    // Linking the allocator object to the freelist functions
    allocator->BackendAlloc = FreelistAlignedAlloc;
    allocator->BackendRealloc = FreelistReAlloc;
    allocator->BackendFree = FreelistFree;
    allocator->backendState = state;
    allocator->parentAllocator = nullptr;

    *out_allocator = allocator;

    REGISTER_ALLOCATOR((u64)arenaStart, (u64)arenaStart + arenaSize, stateSize, &allocator->id, ALLOCATOR_TYPE_GLOBAL, nullptr, name, allocator, true);

    return true;
}

void DestroyGlobalAllocator(Allocator* allocator)
{
    FreelistState* state = (FreelistState*)allocator->backendState;

    UNREGISTER_ALLOCATOR(allocator->id, ALLOCATOR_TYPE_GLOBAL);

    // Frees the entire arena including state
    free(state);
    free(allocator);
}
