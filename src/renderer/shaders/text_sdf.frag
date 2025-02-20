#version 450
#include "defines.glsl"

layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 outColor;

layout(BIND 0) uniform sampler2D tex;


void main() {
	float sdfValue = texture(tex, texCoord).x;
	sdfValue = min(sdfValue, 0.55);
	sdfValue -= 0.45;
	sdfValue *= 10;
	sdfValue = 1 - sdfValue;
	outColor = vec4(sdfValue, sdfValue, sdfValue, sdfValue);
	
	//outColor = vec4(texture(tex, texCoord).xxx, 1);
	//outColor = vec4(texCoord.xy, 0, 1);
}