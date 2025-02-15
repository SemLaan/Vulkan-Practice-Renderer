#version 450

#include "defines.glsl"

layout(location = 0) in vec2 v_modelVertexPosition;

layout(location = 1) in vec4 i_positionAndScale; // .xy = position, .zw = scale
layout(location = 2) in vec4 i_texCoords;

layout(location = 0) out vec2 texCoords;



void main() {
	vec4 position = vec4(v_modelVertexPosition.x * i_positionAndScale.z, v_modelVertexPosition.y * i_positionAndScale.w, 0, 1);
	position.x += i_positionAndScale.x;
	position.y += i_positionAndScale.y;
	//gl_Position = pc.model * position;
	//gl_Position.z = 0.5;
	//gl_Position = pc.model * vec4((v_modelVertexPosition.xy + i_positionAndScale.xy), 0.5, 1);
	vec4 positionThatWorks = vec4((v_modelVertexPosition.xy * i_positionAndScale.zw) + i_positionAndScale.xy, 0.5, 1);
	//positionThatWorks.y = -positionThatWorks.y;
	gl_Position = pc.model * positionThatWorks;
	texCoords = v_modelVertexPosition;
}