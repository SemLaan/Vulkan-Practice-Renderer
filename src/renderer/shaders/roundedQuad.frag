#version 450
#include "defines.glsl"


layout(location = 0) out vec4 outColor;


layout(BIND 1) uniform UniformBufferObject
{
    vec4 color;
} ubo;


void main() {
    outColor = vec4(1, 1, 1, 1);
}