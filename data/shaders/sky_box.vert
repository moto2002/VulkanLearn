#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec3 inPos;

layout (location = 0) out vec3 outSampleDir;

layout (binding = 0) uniform UBO
{
	mat4 model;
	mat4 view;
	mat4 projection;
	mat4 vulkanNDC;
	mat4 mvp;
	vec3 camPos;
	float roughness;
}ubo;

void main() 
{
	mat4 view = ubo.view;
	view[3] = vec4(vec3(0.0), 1.0);
	vec4 pos = ubo.vulkanNDC * ubo.projection * view * vec4(inPos.xyz, 1.0);
	gl_Position = pos.xyww;

	outSampleDir = normalize(vec3(inPos.x, -inPos.y, inPos.z));
}