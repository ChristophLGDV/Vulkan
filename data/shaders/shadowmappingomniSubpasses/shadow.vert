#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec4 inPos;

#define FACE_COUNT 6

layout (binding = 0) uniform UBO 
{
	mat4 mvp[FACE_COUNT];
} ubo;
 
layout(push_constant) uniform PushConsts 
{
	uint faceID;
} pushConsts;
 
out gl_PerVertex 
{
	vec4 gl_Position;   
};

void main()
{
	gl_Position = ubo.mvp[pushConsts.faceID] * inPos;
}