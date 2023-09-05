#version 460 core

layout(early_fragment_tests) in;

layout(location = 0) in FSIn {
	vec2 UV;
} fs_in;

layout(location = 0) out vec4 FragColour;

layout(set = 1, binding = 4) uniform sampler2D Normalmap;

void main() {
	const vec3 normal = textureLod(Normalmap, fs_in.UV, 0.0f).rgb;

	//undo gamma correction because presentation will do it later
	FragColour = vec4(pow(normal, vec3(2.2f)), 1.0f);
}