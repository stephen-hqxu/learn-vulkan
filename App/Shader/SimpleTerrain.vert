#version 460 core
#include "PlaneGeometryAttribute.glsl"

PLANE_ATTRIBUTE_POSITION(0, PlanePosition);
PLANE_ATTRIBUTE_UV(1, UV);

layout(location = 0) out VSOut {
	vec2 UV;
} vs_out;

layout(std430, set = 1, binding = 0) restrict readonly buffer TerrainTransform {
	mat4 Model;	
};

void main() {
	//override input height to make it a flat plane
	//to make sure adaptive LoD calculation in tessellation control shader is correct
	gl_Position = Model * vec4(vec3(PlanePosition.x, 0.0f, PlanePosition.z), 1.0f);
	vs_out.UV = UV;
}