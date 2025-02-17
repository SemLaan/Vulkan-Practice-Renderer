#pragma once
#include "defines.h"

#include "font_loader.h"
#include "text_renderer.h"
#include "math/lin_alg.h"



void CreateGlyphSDF(u8* textureData, u32 textureChannels, u32 textureWidth, u32 textureHeight, Font* font, GlyphData* glyphData, u32 glyphIndex, vec2i bottomLeftTextureCoord, vec2i topRightTextureCoord)
{
	u32 horizontalPixelCount = topRightTextureCoord.x - bottomLeftTextureCoord.x + 1;
	u32 verticalPixelCount = topRightTextureCoord.y - bottomLeftTextureCoord.y + 1;
	u32 charValue = font->renderableCharacters[glyphIndex];

	for (u32 x = 0; x < horizontalPixelCount; x++)
	{
		for (u32 y = 0; y < verticalPixelCount; y++)
		{
			vec2 pixelCenterWorldSpace = vec2_create(((f32)x + 0.5f) / (f32)horizontalPixelCount, ((f32)y + 0.5f) / (f32)verticalPixelCount);
			vec2 pixelCenterFontSpace = vec2_create(
													pixelCenterWorldSpace.x * glyphData->glyphSizes[charValue].x + glyphData->glyphBottomLeftAnchor[charValue].x, 
													pixelCenterWorldSpace.y * glyphData->glyphSizes[charValue].y + glyphData->glyphBottomLeftAnchor[charValue].y);
				
			u32 pixelIndex = ((x + bottomLeftTextureCoord.x) * textureChannels) + ((y + bottomLeftTextureCoord.y) * textureWidth * textureChannels);
			textureData[pixelIndex] = 155;
			textureData[pixelIndex + 1] = 0;
			textureData[pixelIndex + 2] = 0;
		}
	}
}


