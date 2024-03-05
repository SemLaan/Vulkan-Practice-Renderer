#include "darray.h"

#include "core/asserts.h"

// NOTE: Darray struct size (as in the amount of bytes the struct takes up not the size variable) NEEDS to be smaller than DARRAY_MIN_ALIGNMENT
typedef struct Darray
{
	Allocator* 	allocator;		// Allocator used to allocate this array
	void* 		memoryBlock;	// Pointer to the start of the memory block (used for reallocating and freeing)
	u32        	stride;			// Size of each array element
	u32        	size;			// Amount of elements in the array
	u32        	capacity;		// Amount of elements the array can hold
} Darray;


void* DarrayCreate(u32 stride, u32 capacity, Allocator* allocator, MemTag tag)
{
	GRASSERT_DEBUG(sizeof(Darray) < DARRAY_MIN_ALIGNMENT);

	// Allocating memory for the darray elements + state
	void* block = AlignedAlloc(allocator, DARRAY_MIN_ALIGNMENT + (capacity * stride), DARRAY_MIN_ALIGNMENT, tag);

	// Getting a pointer to where the elements of the array will be stored
	void* elements = (u8*)block + DARRAY_MIN_ALIGNMENT;

	// Storing array state in the memory before the elements
	Darray* state = (Darray*)elements - 1;
	state->allocator = allocator;
	state->memoryBlock = block;
	state->stride = stride;
	state->size = 0;
	state->capacity = capacity;

	// Returning a pointer to only the elements
	return elements;
}

// Same as darray create except size is set to the same as capacity
void* DarrayCreateWithSize(u32 stride, u32 capacityAndSize, Allocator* allocator, MemTag tag)
{
	void* block = AlignedAlloc(allocator, DARRAY_MIN_ALIGNMENT + (capacityAndSize * stride), DARRAY_MIN_ALIGNMENT, tag);

	void* elements = (u8*)block + DARRAY_MIN_ALIGNMENT;

	Darray* state = (Darray*)elements - 1;
	state->allocator = allocator;
	state->memoryBlock = block;
	state->stride = stride;
	state->size = capacityAndSize;
	state->capacity = capacityAndSize;

	return elements;
}

void* DarrayPushback(void* elements, void* element)
{
	// Getting a pointer to the state data
	Darray* state = (Darray*)elements - 1;

	// If there is no more room for another element, increase the capacity of the array
	if (state->size >= state->capacity)
	{
		state->capacity = (u32)(state->capacity * DARRAY_SCALING_FACTOR + 1);
		void* temp = Realloc(state->allocator, state->memoryBlock, (state->stride * state->capacity) + DARRAY_MIN_ALIGNMENT);
		elements = (u8*)temp + DARRAY_MIN_ALIGNMENT;
		state = (Darray*)elements - 1;
		state->memoryBlock = temp;
	}

	// Copy the element data into the next free slot in the array
	MemoryCopy((u8*)elements + (state->stride * state->size), element, (size_t)state->stride);
	state->size++;
	return elements;
}

void DarrayPop(void* elements)
{
	Darray* state = (Darray*)elements - 1;
	state->size--;
}

void DarrayPopAt(void* elements, u32 index)
{
	Darray* state = (Darray*)elements - 1;

	// Copy all the elements that come after the popped element downward to fill the hole in the array
	u8* address = (u8*)elements + (index * state->stride);
	MemoryCopy(address, address + state->stride, (state->size - 1 - index) * state->stride);

	state->size--;
}

void DarrayDestroy(void* elements)
{
	Darray* state = (Darray*)elements - 1;
	Free(state->allocator, state->memoryBlock);
}

u32 DarrayGetSize(void* elements)
{
	Darray* state = (Darray*)elements - 1;
	return state->size;
}

void DarraySetSize(void* elements, u32 size)
{
	Darray* state = (Darray*)elements - 1;
	state->size = size;
}

bool DarrayContains(void* elements, void* element)
{
	Darray* state = (Darray*)elements - 1;

	// Looping through the array to find the element
	for (u32 i = 0; i < state->size; i++)
	{
		if (MemoryCompare((u8*)elements + (i * state->stride), element, state->stride))
			return true;
	}

	return false;
}

u32 DarrayGetElementIndex(void* elements, void* element)
{
	Darray* state = (Darray*)elements - 1;

	// Looping through the array to find the element
	for (u32 i = 0; i < state->size; i++)
	{
		if (MemoryCompare((u8*)elements + (i * state->stride), element, state->stride))
			return i;
	}

	_ERROR("Darray: tried to get element index of non-existent element");
	return UINT32_MAX;
}
