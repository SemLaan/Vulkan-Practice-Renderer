#pragma once
#include "defines.h"
#include "core/meminc.h"


// This makes sure arrays are cache line aligned on most consumer hardware
#define DARRAY_MIN_ALIGNMENT 64

// Scaling factor for when a darray doesn't have enough memory anymore
#define DARRAY_SCALING_FACTOR 1.6f


// Creates a dynamic array
void* DarrayCreate(u32 stride, u32 capacity, Allocator* allocator, MemTag tag);
// Creates a dynamic array with a certain amount of elements already reserved
void* DarrayCreateWithSize(u32 stride, u32 capacityAndSize, Allocator* allocator, MemTag tag);

// WARNING: this invalidates the old elements pointer so be carefull when using the darray in 2 places
// the pointer might become invalidated
void* DarrayPushback(void* elements, void* element);

// Removes the top element
void DarrayPop(void* elements);

// Removes the element at the given index and moves all elements above it down to fill the gap
void DarrayPopAt(void* elements, u32 index);

void DarrayDestroy(void* elements);

// Returns the amount of used elements in the darray
u32 DarrayGetSize(void* elements);
void DarraySetSize(void* elements, u32 size);

// Does a linear search through the array to find the element
// Only use on small arrays because performance is poor, consider a hash map
bool DarrayContains(void* elements, void* element);

// Does a linear search through the array to find the element
// Only use on small arrays because performance is poor, consider a hash map
u32 DarrayGetElementIndex(void* elements, void* element);
