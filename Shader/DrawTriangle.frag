#version 460 core

layout(early_fragment_tests) in;

layout(location = 0) in vec2 FragUV;

layout(location = 0) out vec4 FragColour;

layout(set = 1, binding = 0) uniform sampler2D SurfaceTexture;

void main() {
	FragColour = vec4(texture(SurfaceTexture, FragUV).rgb, 1.0f);
}