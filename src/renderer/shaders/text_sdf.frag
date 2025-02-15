#version 450
#include "defines.glsl"

layout(location = 0) in vec2 texCoords;

layout(location = 0) out vec4 outColor;


void main() {
    outColor = vec4(texCoords.xy, 0, 1);
}