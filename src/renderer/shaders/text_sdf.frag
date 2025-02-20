#version 450
#include "defines.glsl"

layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 outColor;

layout(BIND 0) uniform sampler2D tex;


void main() {
	float sdfValue = texture(tex, texCoord).x;
	outColor = vec4(0, 0, 0, 0);
	if (sdfValue < 0.5)
		outColor = vec4(1, 1, 1, 1);
	
	//outColor = vec4(texture(tex, texCoord).xxx, 1);
	//outColor = vec4(texCoord.xy, 0, 1);
}