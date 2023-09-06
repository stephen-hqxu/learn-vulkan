#ifndef _PLANE_GEOMETRY_GLSL_
#define _PLANE_GEOMETRY_GLSL_
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_float64 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#ifndef PLANE_HIDE_PLANE_PROPERTY
layout(std430, set = 0, binding = 0) readonly restrict buffer PlaneProperty {
	dvec2 Dimension, TotalPlane;
	uvec2 Subdivision, VertexDimension;
	uint32_t IndexCount;
};

//Convert 2D location of a vertex to 1D index (this index is not the geometry index, btw).
uint calcVertexIndex(const uvec2 id) {
	return id.x + id.y * VertexDimension.x;
}
#endif//PLANE_HIDE_PLANE_PROPERTY

#ifdef PLANE_VERTEX_ACCESS
layout(std430, buffer_reference, buffer_reference_align = 4) PLANE_VERTEX_ACCESS restrict buffer PlaneVertex {
	vec3 Position;/**< RGB32F */
	u16vec2 UV;/**< RG16 */
};
#endif//PLANE_VERTEX_ACCESS
#ifdef PLANE_INDEX_ACCESS
layout(std430, buffer_reference, buffer_reference_align = 4) PLANE_INDEX_ACCESS restrict buffer PlaneIndex {
	uint32_t Index[6];
};
#endif//PLANE_INDEX_ACCESS
#ifdef PLANE_COMMAND_ACCESS
layout(std430, buffer_reference, buffer_reference_align = 4) PLANE_COMMAND_ACCESS restrict buffer PlaneCommand {
	//See Vulkan specification to find out the structure of indirect index data block.
	uint32_t A, B, C;
	int32_t D;
	uint32_t E;
};
#endif//PLANE_COMMAND_ACCESS

#endif//_PLANE_GEOMETRY_GLSL_