#pragma once
#include "defines.h"
#include "core/meminc.h"


typedef struct CircularQueue
{
	void* data;				// Backing memory for the queue
	Allocator* allocator;	// Allocator used to allocate this queue
	u32 front;
	u32 rear;
	u32 size;				// Amount of elements in the queue, shouldn't exceed capacity
	u32 capacity;			// Amount of elements the queue can hold
	u32 stride;				// Size of each queue element
} CircularQueue;


void CircularQueueCreate(void* out_circularQueue, u32 capacity, u32 stride, Allocator* allocator);
void CircularQueueDestroy(void* circularQueue);

void CircularQueueEnqueue(void* circularQueue, void* ptrToElement);
void CircularQueueDequeue(void* circularQueue);


// Typedefs a struct that is the same as the Circular queue struct, except it's data pointer is type* instead of void*
// Also defines some helper function wrappers
#define DEFINE_CIRCULARQUEUE_TYPE(type) \
typedef struct type ## CircularQueue \
{ \
type* data; \
Allocator* allocator;  \
u32 front; \
u32 rear; \
u32 size; \
u32 capacity; \
u32 stride; \
} type ## CircularQueue; \
\
\
inline static void type ## CircularQueueCreate(type ## CircularQueue* out_circularQueue, u32 capacity, Allocator* allocator) { return CircularQueueCreate(out_circularQueue, capacity, sizeof(type), allocator); }\
inline static void type ## CircularQueueEnqueue(type ## CircularQueue* queue, type* ptrToElement) { CircularQueueEnqueue(queue, ptrToElement); }\

