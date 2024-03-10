#pragma once
#include "defines.h"
#include "buffer.h"


// Loads the vertices and indices of an obj mesh file into an index and vertex buffer
bool LoadObj(const char* filename, VertexBuffer* out_vb, IndexBuffer* out_ib, bool flipWindingOrder);
