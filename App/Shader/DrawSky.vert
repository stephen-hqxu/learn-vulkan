#version 460 core
#include "CameraData.glsl"

layout(location = 0) out vec3 RayDirection;

/*
This is a tiny optimised technique for drawing a full-screen quad.
It not only reduces the amount of data passed to the rasteriser,
but also eliminates the chances to demote fragment invocation.
*/
const vec2 QuadVertex[] = vec2[](
	vec2(-1.0f, -1.0f),
	vec2(3.0f, -1.0f),
	vec2(-1.0f, 3.0f)
);

void main() {
	/*
	This is a little hack for rendering a skybox without a box.
	All info we need is the 3D ray direction for sampling from the cubemap,
	and what we can do is by traversing the primitive transition pipeline backward,
	i.e., NDC -> clip -> view(rotation only) -> world.
	We treat the quad as if infinitely far away, so any previously drawn geometries will not be overwritten.

	Just a heads-up: recall that we are using reversed depth, so 0.0f will be the infinite far.
	When the scene has no geometry drawn, the depth in the attachment will be the initial value (0.0f),
	and we shall let depth test pass for sky pixels, therefore depth test needs to have compare mode set to equal.
	*/
	const vec4 inf_quad = vec4(QuadVertex[gl_VertexIndex], 0.0f, 0.0f);
	RayDirection = (Camera.InvProjectionViewRotation * inf_quad).xyz;

	gl_Position = inf_quad;
}