// This file implements both of these headers!
#include "vulkan_buffer.h"
#include "../buffer.h"

#include "core/asserts.h"
#include "vulkan_command_buffer.h"
#include "vulkan_memory.h"
#include "vulkan_types.h"
#include "vulkan_transfer.h"
#include "vulkan_utils.h"



VertexBuffer VertexBufferCreate(void* vertices, size_t size)
{
	// Allocating memory for the buffer (struct on CPU not vulkan)
	VertexBuffer clientBuffer;
	clientBuffer.internalState = Alloc(vk_state->rendererAllocator, sizeof(VulkanVertexBuffer));
	VulkanVertexBuffer* buffer = (VulkanVertexBuffer*)clientBuffer.internalState;
	buffer->size = size;

	// ================= creating the GPU buffer =========================
	BufferCreate(buffer->size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, MemType(MEMORY_TYPE_STATIC), &buffer->handle, &buffer->memory);

	if (vertices != nullptr)
	{
		// ================ Staging buffer =========================
		VkBuffer stagingBuffer;
		VulkanAllocation stagingAllocation;

		BufferCreate(buffer->size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MemType(MEMORY_TYPE_UPLOAD), &stagingBuffer, &stagingAllocation);

		// ================= copying data into staging buffer ===============================
		CopyDataToAllocation(&stagingAllocation, vertices, 0, buffer->size);

		// ================= copying data into GPU buffer ===============================
		VulkanBufferCopyData bufferCopy = {};
		bufferCopy.srcBuffer = stagingBuffer;
		bufferCopy.dstBuffer = buffer->handle;
		bufferCopy.copyRegion.srcOffset = 0;
		bufferCopy.copyRegion.dstOffset = 0;
		bufferCopy.copyRegion.size = size;

		RequestBufferUpload(&bufferCopy, TRANSFER_METHOD_UNSYNCHRONIZED);

		// Making sure the staging buffer and memory get deleted
		QueueDeferredBufferDestruction(stagingBuffer, &stagingAllocation, DESTRUCTION_TIME_NEXT_FRAME);
	}

	return clientBuffer;
}

// Updates the data in the vertex buffer, note that size can't be larger than the size of the buffer
void VertexBufferUpdate(VertexBuffer clientBuffer, void* vertices, u64 size)
{
	// TODO: this is a slow copy, an option should be added to vertex buffer create that creates 2 buffers and an upload buffer so data can be uploaded without halting the cpu and gpu
	VulkanVertexBuffer* buffer = clientBuffer.internalState;
	GRASSERT_MSG(size <= buffer->size, "Tried to update vertex buffer with more vertices than vertex buffer can hold");

	// ================ Staging buffer =========================
	VkBuffer stagingBuffer;
	VulkanAllocation stagingAllocation;

	BufferCreate(buffer->size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MemType(MEMORY_TYPE_UPLOAD), &stagingBuffer, &stagingAllocation);

	// ================= copying data into staging buffer ===============================
	CopyDataToAllocation(&stagingAllocation, vertices, 0, size);

	// ================= copying data into GPU buffer ===============================
	VulkanBufferCopyData bufferCopy = {};
	bufferCopy.srcBuffer = stagingBuffer;
	bufferCopy.dstBuffer = buffer->handle;
	bufferCopy.copyRegion.srcOffset = 0;
	bufferCopy.copyRegion.dstOffset = 0;
	bufferCopy.copyRegion.size = size;

	RequestBufferUpload(&bufferCopy, TRANSFER_METHOD_SYNCHRONIZED_SINGLE_BUFFERED);

	// Making sure the staging buffer and memory get deleted
	QueueDeferredBufferDestruction(stagingBuffer, &stagingAllocation, DESTRUCTION_TIME_NEXT_FRAME);
}

void VertexBufferDestroy(VertexBuffer clientBuffer)
{
	VulkanVertexBuffer* buffer = (VulkanVertexBuffer*)clientBuffer.internalState;

	// Making sure the staging buffer and memory get deleted
	QueueDeferredBufferDestruction(buffer->handle, &buffer->memory, DESTRUCTION_TIME_CURRENT_FRAME);

	Free(vk_state->rendererAllocator, buffer);
}

IndexBuffer IndexBufferCreate(u32* indices, size_t indexCount)
{
	IndexBuffer clientBuffer;
	clientBuffer.internalState = Alloc(vk_state->rendererAllocator, sizeof(VulkanIndexBuffer));
	VulkanIndexBuffer* buffer = (VulkanIndexBuffer*)clientBuffer.internalState;
	buffer->size = indexCount * sizeof(u32);
	buffer->indexCount = indexCount;

	// ================ Staging buffer =========================
	VkBuffer stagingBuffer;
	VulkanAllocation stagingAllocation;

	BufferCreate(buffer->size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MemType(MEMORY_TYPE_UPLOAD), &stagingBuffer, &stagingAllocation);

	// ================= copying data into staging buffer ===============================
	CopyDataToAllocation(&stagingAllocation, indices, 0, buffer->size);

	// ================= creating the actual buffer =========================
	BufferCreate(buffer->size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, MemType(MEMORY_TYPE_STATIC), &buffer->handle, &buffer->memory);

	// ================= copying data into GPU buffer ===============================
	VulkanBufferCopyData bufferCopy = {};
	bufferCopy.srcBuffer = stagingBuffer;
	bufferCopy.dstBuffer = buffer->handle;
	bufferCopy.copyRegion.srcOffset = 0;
	bufferCopy.copyRegion.dstOffset = 0;
	bufferCopy.copyRegion.size = buffer->size;

	RequestBufferUpload(&bufferCopy, TRANSFER_METHOD_UNSYNCHRONIZED);

	// Making sure the staging buffer and memory get deleted
	QueueDeferredBufferDestruction(stagingBuffer, &stagingAllocation, DESTRUCTION_TIME_NEXT_FRAME);

	return clientBuffer;
}

void IndexBufferDestroy(IndexBuffer clientBuffer)
{
	VulkanIndexBuffer* buffer = (VulkanIndexBuffer*)clientBuffer.internalState;

	// Making sure the staging buffer and memory get deleted
	QueueDeferredBufferDestruction(buffer->handle, &buffer->memory, DESTRUCTION_TIME_CURRENT_FRAME);

	Free(vk_state->rendererAllocator, buffer);
}
