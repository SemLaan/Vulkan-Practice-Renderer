#version 450
#include "defines.glsl"

layout(location = 0) in vec2 v_bezierPosition; // x = distance along bezier, y = above or below bezier (1 or -1)
layout(location = 1) in vec4 i_beginEndPoints;
layout(location = 2) in vec2 i_midPoint;

layout(BIND 0) uniform UniformBufferObject
{
    float lineThickness;
} ubo;

vec2 BezierInterpolation(vec2 p0, vec2 p1, vec2 p2, float t)
{
    vec2 intermediate1 = mix(p0, p1, t);
    vec2 intermediate2 = mix(p1, p2, t);
    return mix(intermediate1, intermediate2, t);
}

void main() {
    vec2 bezierPosition = BezierInterpolation(i_beginEndPoints.xy, i_midPoint, i_beginEndPoints.zw, v_bezierPosition.x);
    vec2 bezierPositionPlus = BezierInterpolation(i_beginEndPoints.xy, i_midPoint, i_beginEndPoints.zw, v_bezierPosition.x + 0.1);
    vec2 bezierTangent = normalize(bezierPositionPlus - bezierPosition);
    vec2 bezierNormal = vec2(-bezierTangent.y, bezierTangent.x);
    bezierPosition += bezierNormal * v_bezierPosition.y * 0.015;// TODO: multiply with line thickness
	gl_Position = pc.model * vec4(bezierPosition, 0, 1);
}