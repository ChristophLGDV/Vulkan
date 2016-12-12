#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
 
layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inViewVec;
layout (location = 4) in vec3 inLightVec;
 
struct SceneMaterialProperites {
	vec4 ambient;
	vec3 diffuse;
	float opacity;
	vec4 specular;
};

layout (set = 1,  binding = 0) uniform  MaterialDataBuffer{
	SceneMaterialProperites material[15];
};
layout (set = 1, binding = 1) uniform sampler samplerColorMap;

layout (set = 1, binding = 2) uniform sampler2D diffuseTextures[15]; 


layout(push_constant) uniform PushConsts {
	int matID;
} pushConsts;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	vec4 color = texture(diffuseTextures[pushConsts.matID] , inUV) * vec4(inColor, 1.0);
	vec3 N = normalize(inNormal);
	vec3 L = normalize(inLightVec);
	vec3 V = normalize(inViewVec);
	vec3 R = reflect(-L, N);
	vec3 diffuse = max(dot(N, L), 0.0) * material[pushConsts.matID].diffuse.rgb;
	vec3 specular = pow(max(dot(R, V), 0.0), 16.0) * material[pushConsts.matID].specular.rgb;
	outFragColor = vec4((material[pushConsts.matID].ambient.rgb + diffuse) * color.rgb + specular, 1.0-material[pushConsts.matID].opacity);
 
}