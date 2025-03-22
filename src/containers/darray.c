#include "darray.h"

#include "core/asserts.h"

void* DarrayCreate(u32 stride, u32 startCapacity, Allocator* allocator)
{
    Darray* darray = Alloc(allocator, sizeof(*darray));

    darray->data = AlignedAlloc(allocator, stride * startCapacity, DARRAY_MIN_ALIGNMENT);
	darray->allocator = allocator;
    darray->size = 0;
    darray->capacity = startCapacity;
    darray->stride = stride;

    return darray;
}

void* DarrayCreateWithSize(u32 stride, u32 startCapacityAndSize, Allocator* allocator)
{
    Darray* darray = Alloc(allocator, sizeof(*darray));

    darray->data = AlignedAlloc(allocator, stride * startCapacityAndSize, DARRAY_MIN_ALIGNMENT);
	darray->allocator = allocator;
    darray->size = startCapacityAndSize;
    darray->capacity = startCapacityAndSize;
    darray->stride = stride;

    return darray;
}

void DarrayDestroy(void* darray)
{
    Darray* state = (Darray*)darray;

    Free(state->allocator, state->data);
    Free(state->allocator, state);
}

void DarrayPushback(void* darray, void* ptrToElement)
{
    Darray* state = (Darray*)darray;

    // Check if the array needs more memory, if so use the scaling factor to determine how much
    if (state->size >= state->capacity)
    {
        state->capacity = (u32)(state->capacity * DARRAY_SCALING_FACTOR + 1);
        state->data = Realloc(state->allocator, state->data, state->capacity * state->stride);
    }

    MemoryCopy((u8*)state->data + (state->stride * state->size), ptrToElement, (size_t)state->stride);
    state->size++;
}

void DarrayPop(void* darray)
{
    Darray* state = (Darray*)darray;
    state->size--;
}

void DarrayPopAt(void* darray, u32 index)
{
    Darray* state = (Darray*)darray;

    // Copy all the elements that come after the popped element downward to fill the hole in the array
    u8* address = (u8*)state->data + (index * state->stride);
    MemoryCopy(address, address + state->stride, (state->size - 1 - index) * state->stride);

    state->size--;
}

void DarrayPopRange(void* darray, u32 firstIndex, u32 count)
{
	Darray* state = (Darray*)darray;

	GRASSERT_DEBUG(state->size >= firstIndex + count);

	u8* address = (u8*)state->data + (firstIndex * state->stride);
    MemoryCopy(address, address + (state->stride * count), (state->size - (firstIndex + count)) * state->stride);

	state->size -= count;
}

// Sets the size value of the darray, increases capacity if necessary
void DarraySetSize(void* darray, u32 size)
{
    Darray* state = (Darray*)darray;

    if (size > state->capacity)
    {
        state->capacity = size;
        state->data = Realloc(state->allocator, state->data, state->capacity * state->stride);
    }
    state->size = size;
}

void DarraySetCapacity(void* darray, u32 newCapacity)
{
	Darray* state = (Darray*)darray;

	GRASSERT(state->size <= newCapacity);

	state->data = Realloc(state->allocator, state->data, newCapacity * state->stride);
	state->capacity = newCapacity;
}

// Reallocs the data to make the capacity the same as size.
void DarrayFitExact(void* darray)
{
	Darray* state = (Darray*)darray;

	if (state->capacity != state->size)
	{
		state->capacity = state->size;
		state->data = Realloc(state->allocator, state->data, state->capacity * state->stride);
	}
}

// Does a linear search through the array to find the element
// Only use on small arrays because performance is poor, consider a hash map
bool DarrayContains(void* darray, void* ptrToElement)
{
	Darray* state = (Darray*)darray;

	// Looping through the array to find the element
    for (u32 i = 0; i < state->size; i++)
    {
        if (MemoryCompare((u8*)state->data + (i * state->stride), ptrToElement, state->stride))
            return true;
    }

    return false;
}

// Does a linear search through the array to find the element
// Only use on small arrays because performance is poor, consider a hash map
u32 DarrayGetElementIndex(void* darray, void* ptrToElement)
{
	Darray* state = (Darray*)darray;

	// Looping through the array to find the element
    for (u32 i = 0; i < state->size; i++)
    {
        if (MemoryCompare((u8*)state->data + (i * state->stride), ptrToElement, state->stride))
            return i;
    }

    _ERROR("Darray: tried to get element index of non-existent element");
    return UINT32_MAX;
}

