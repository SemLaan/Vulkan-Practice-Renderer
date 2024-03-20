#version 450
#include "defines.glsl"


layout(location = 0) out vec4 outColor;


layout(BIND 0) uniform UniformBufferObject
{
    vec4 color;
} ubo;


void main() {
    outColor = ubo.color;
}