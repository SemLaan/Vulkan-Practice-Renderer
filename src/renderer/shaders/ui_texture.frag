#version 450

#include "defines.glsl"

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 texCoord;


layout(BIND 1) uniform sampler2D tex;

void main() 
{
    float depth = (1 - texture(tex, vec2(texCoord.x, 1-texCoord.y)).r) * 255;
    outColor = vec4(depth, depth, depth, 1);
    //outColor = texture(tex, texCoord);
}