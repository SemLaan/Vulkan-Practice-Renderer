#include "circular_queue.h"

#include "core/asserts.h"

void CircularQueueCreate(void* out_circularQueue, u32 capacity, u32 stride, Allocator* allocator)
{
	CircularQueue* queue = out_circularQueue;
	queue->allocator = allocator;
	queue->data = AlignedAlloc(allocator, capacity * stride, CACHE_ALIGN);
	queue->size = 0;
	queue->stride = stride;
	queue->capacity = capacity;
	queue->front = 0;
	queue->rear = 0;
}

void CircularQueueDestroy(void* circularQueue)
{
	CircularQueue* queue = circularQueue;
	Free(queue->allocator, queue->data);
}

void CircularQueueEnqueue(void* circularQueue, void* ptrToElement)
{
	CircularQueue* queue = circularQueue;
	if (queue->size == 0)
	{
		MemoryCopy(queue->data, ptrToElement, queue->stride);
		queue->front = 0;
		queue->rear = 0;
		queue->size = 1;
	}
	else // if queue size is greater than zero
	{
		queue->size++;
		GRASSERT(queue->size <= queue->capacity);
		queue->front++;
		queue->front %= queue->capacity;
		MemoryCopy((u8*)queue->data + (queue->front * queue->stride), ptrToElement, queue->stride);
	}
}

void CircularQueueDequeue(void* circularQueue)
{
	CircularQueue* queue = circularQueue;
	GRASSERT_DEBUG(queue->size > 0);
	queue->rear++;
	queue->rear %= queue->capacity;
}



