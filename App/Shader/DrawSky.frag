#version 460 core

layout(early_fragment_tests) in;

layout(location = 0) in vec3 RayDirection;

layout(location = 0) out vec4 FragColour;

layout(set = 1, binding = 0) uniform samplerCube SkyBox;

void main() {
	FragColour = vec4(textureLod(SkyBox, normalize(RayDirection), 0.0f).rgb, 1.0f);
}