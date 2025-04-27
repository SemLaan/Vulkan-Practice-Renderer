#version 450
#include "defines.glsl"

layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 outColor;

layout(BIND 0) uniform sampler2D tex;


void main() {
	float sdfValue = texture(tex, texCoord).x;
	
	float halfThresholdSize = 0.2 / 2.0;
	float thresholdStart = 0.5 - halfThresholdSize;
	float thresholdProgress = (sdfValue - thresholdStart) / 0.2;
	outColor = vec4(clamp(mix(1, 0, thresholdProgress), 0, 1));
}