#version 420

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#define FACE_COUNT 6

layout (triangles, invocations = FACE_COUNT) in;
layout (triangle_strip, max_vertices = 3) out;


layout (binding = 0) uniform UBO 
{
	mat4 mvp[FACE_COUNT];
} ubo;
 

void main() 
{
  
	for (int i = 0; i < gl_in.length(); i++)
	{
		gl_Layer = gl_InvocationID; 
		gl_Position = ubo.mvp[gl_InvocationID] *  gl_in[i].gl_Position ; 
		EmitVertex();
	}
	EndPrimitive();
}