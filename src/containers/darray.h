#pragma once
#include "defines.h"
#include "core/meminc.h"

// This makes sure arrays are cache line aligned on most consumer hardware
#define DARRAY_MIN_ALIGNMENT 64

// Scaling factor for when a darray doesn't have enough memory anymore
#define DARRAY_SCALING_FACTOR 1.6f

typedef struct Darray
{
	void* data;				// Actual array
	Allocator* allocator;	// Allocator used to allocate this array
	u32 size;				// Amount of elements in the array, shouldn't exceed capacity
	u32 capacity;			// Amount of elements the array can hold
	u32 stride;				// Size of each array element
} Darray;


void* DarrayCreate(u32 stride, u32 startCapacity, Allocator* allocator);
void* DarrayCreateWithSize(u32 stride, u32 startCapacityAndSize, Allocator* allocator);
void DarrayDestroy(void* darray);

void DarrayPushback(void* darray, void* ptrToElement);
void DarrayPop(void* darray);
void DarrayPopAt(void* darray, u32 index);
void DarrayPopRange(void* darray, u32 firstIndex, u32 count);

// Sets the size value of the darray, increases capacity if necessary
void DarraySetSize(void* darray, u32 size);
// Sets the darray's capacity, used for lowering its capacity
void DarraySetCapacity(void* darray, u32 newCapacity);
// Reallocs the data to make the capacity the same as size.
void DarrayFitExact(void* darray);

// Does a linear search through the array to find the element
// Only use on small arrays because performance is poor, consider a hash map
bool DarrayContains(void* darray, void* ptrToElement);

// Does a linear search through the array to find the element
// Only use on small arrays because performance is poor, consider a hash map
u32 DarrayGetElementIndex(void* darray, void* ptrToElement);


// Typedefs a struct that is the same as the Darray struct, except it's data pointer is type* instead of void*
// Also defines some helper function wrappers
#define DEFINE_DARRAY_TYPE(type) \
typedef struct type ## Darray \
{ \
type* data; \
Allocator* allocator;  \
u32 size; \
u32 capacity; \
u32 stride; \
} type ## Darray; \
\
\
inline static type ## Darray* type ## DarrayCreate(u32 startCapacity, Allocator* allocator) { return DarrayCreate(sizeof(type), startCapacity, allocator); }\
inline static type ## Darray* type ## DarrayCreateWithSize(u32 startCapacityAndSize, Allocator* allocator) { return DarrayCreateWithSize(sizeof(type), startCapacityAndSize, allocator); }\
inline static void type ## DarrayPushback(type ## Darray* darray, type* ptrToElement) { DarrayPushback(darray, ptrToElement); }\
inline static bool type ## DarrayContains(type ## Darray* darray, type* ptrToElement) { return DarrayContains(darray, ptrToElement); } \
inline static u32 type ## DarrayGetElementIndex(type ## Darray* darray, type* ptrToElement) { return DarrayGetElementIndex(darray, ptrToElement); }



// Typedefs a struct that is the same as the Darray struct, except it's data pointer is type** instead of void*
// Also defines some helper function wrappers
#define DEFINE_DARRAY_TYPE_REF(type) \
typedef struct type ## RefDarray \
{ \
type** data; \
Allocator* allocator;  \
u32 size; \
u32 capacity; \
u32 stride; \
} type ## RefDarray; \
\
\
inline static type ## RefDarray* type ## RefDarrayCreate(u32 startCapacity, Allocator* allocator) { return DarrayCreate(sizeof(type*), startCapacity, allocator); }\
inline static type ## RefDarray* type ## RefDarrayCreateWithSize(u32 startCapacityAndSize, Allocator* allocator) { return DarrayCreateWithSize(sizeof(type*), startCapacityAndSize, allocator); }\
inline static void type ## RefDarrayPushback(type ## RefDarray* darray, type** ptrToElement) { DarrayPushback(darray, ptrToElement); }\
inline static bool type ## RefDarrayContains(type ## RefDarray* darray, type** ptrToElement) { return DarrayContains(darray, ptrToElement); } \
inline static u32 type ## RefDarrayGetElementIndex(type ## RefDarray* darray, type** ptrToElement) { return DarrayGetElementIndex(darray, ptrToElement); }


