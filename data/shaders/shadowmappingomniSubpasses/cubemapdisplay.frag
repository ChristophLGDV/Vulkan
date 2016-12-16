#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (binding = 1) uniform samplerCubeShadow shadowCubeMap;

layout (location = 0) in vec3 inUVW;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	float z = texture(shadowCubeMap, vec4(inUVW,.99)).r;
	// shadowcomparison prohibits regular sampling

	outFragColor = vec4(vec3(z), 1.0);
}