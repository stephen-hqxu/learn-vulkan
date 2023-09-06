#version 460 core

#include "CameraData.glsl"

layout(vertices = 3) out;

layout(location = 0) in TECIn {
	vec2 UV;
} tec_in[];

layout(location = 0) out TECOut {
	vec2 UV;
} tec_out[];

layout(std430, set = 1, binding = 1) restrict readonly buffer TessellationSetting {
	float MaxLod, MinLod, MaxDistance, MinDistance;
};

const uvec2 PatchEdgeIndex[3] = {
	{ 1u, 2u },
	{ 2u, 0u },
	{ 0u, 1u }
};

float calcLoD(const float v1, const float v2) {
	return mix(MaxLod, MinLod, (v1 + v2) * 0.5f);
}

void main() {
	//pass-through
	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
	tec_out[gl_InvocationID].UV = tec_in[gl_InvocationID].UV;

	//compute tessellation level in parallel
	const uvec2 current_edge = PatchEdgeIndex[gl_InvocationID];
	float vertex_distance[2];
	//first calculate the distance from camera to each vertex in a patch
	for (uint i = 0u; i < vertex_distance.length(); i++) {
		//override the altitude of view position and vertex position
		//to make sure they are at the same height and will not be affected by displacement of vertices later.
		const vec2 vertex_pos = gl_in[current_edge[i]].gl_Position.xz,
			view_pos = Camera.Position.xz;

		//perform linear interpolation
		vertex_distance[i] = clamp((distance(vertex_pos, view_pos) - MinDistance) / (MaxDistance - MinDistance), 0.0f, 1.0f);
	}
	//each invocation (3 in total) is responsible for an outer level
	gl_TessLevelOuter[gl_InvocationID] = calcLoD(vertex_distance[0], vertex_distance[1]);
	barrier();
	//the first invocation sync from other threads and compute the inner level
	if (gl_InvocationID == 0u) {
		gl_TessLevelInner[0] = (gl_TessLevelOuter[0] + gl_TessLevelOuter[1] + gl_TessLevelOuter[2]) / 3.0f;
	}
}