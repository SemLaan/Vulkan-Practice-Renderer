#pragma once
#include "defines.h"
#include "renderer_types.h"


typedef enum TextureStorageType
{
	TEXTURE_STORAGE_RGBA8UNORM,
	TEXTURE_STORAGE_RGBA8SRGB,
} TextureStorageType;

// Creates a texture with width and height from the pixels array, pixels array is copied.
// Pixels are assumed to be 32 bits, 8 bits per channel rgba
Texture TextureCreate(u32 width, u32 height, void* pixels, TextureStorageType textureStorageType, bool mipmapped);
void TextureDestroy(Texture clientTexture);
