#pragma once
#include "defines.h"
#include "containers/darray.h"
#include "core/meminc.h"
#include "renderer_types.h"


// Creates a vertex buffer handle to an object with the given vertices, size is in bytes
VertexBuffer VertexBufferCreate(void* vertices, size_t size);
// Updates the given vertex buffer with new vertices, the new size is expected to be less than or equal to the size of the buffer
// if the size is less than the full buffer, what happens to the rest of the buffer is undefined *this may change in the future and I may forget to update this comment*
void VertexBufferUpdate(VertexBuffer clientBuffer, void* vertices, u64 size);
// Destroys the vertex buffer of the associated handle
void VertexBufferDestroy(VertexBuffer clientBuffer);

/// <summary>
/// Creates an index buffer
/// </summary>
/// <param name="indices">Array of indices, type needs to be u32</param>
/// <param name="indexCount"></param>
/// <returns></returns>
IndexBuffer IndexBufferCreate(u32* indices, size_t indexCount);
void IndexBufferDestroy(IndexBuffer clientBuffer);