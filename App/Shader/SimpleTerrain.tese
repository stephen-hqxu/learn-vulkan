#version 460 core

#include "CameraData.glsl"

layout(triangles, fractional_even_spacing, ccw) in;

layout(location = 0) in TEEIn {
	vec2 UV;
} tee_in[];

layout(location = 0) out TEEOut {
	vec2 UV;
} tee_out;

layout(std430, set = 1, binding = 2) restrict readonly buffer DisplacementSetting {
	float Altitude;
};

layout(set = 1, binding = 3) uniform sampler2D Heightfield;

vec2 toCartesian2D(const vec2 v1, const vec2 v2, const vec2 v3){
	return vec2(gl_TessCoord.x) * v1 + vec2(gl_TessCoord.y) * v2 + vec2(gl_TessCoord.z) * v3;
}

vec4 toCartesian4D(const vec4 v1, const vec4 v2, const vec4 v3){
	return vec4(gl_TessCoord.x) * v1 + vec4(gl_TessCoord.y) * v2 + vec4(gl_TessCoord.z) * v3;
}

void main() {
	//interpolate Barycentric to Cartesian
	gl_Position = toCartesian4D(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_in[2].gl_Position);
	tee_out.UV = toCartesian2D(tee_in[0].UV, tee_in[1].UV, tee_in[2].UV);

	//our plane is always pointing upwards
	//displace the terrain, moving the vertices upward
	gl_Position.y += textureLod(Heightfield, tee_out.UV, 0.0f).a * Altitude;

	gl_Position = Camera.ProjectionView * gl_Position;
}