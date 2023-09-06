#version 460 core
#include "SimpleWater.glsl"
#include "CameraData.glsl"
#include "PlaneGeometryAttribute.glsl"

PLANE_ATTRIBUTE_POSITION(0, WaterPosition);
PLANE_ATTRIBUTE_UV(1, WaterUV);

WATER_RAY_PROPERTY(out);

void main() {
	vec4 position_world = Water.Model * vec4(WaterPosition, 1.0f);
	position_world.y += Water.AltitudeOffset;

	gl_Position = Camera.ProjectionView * position_world;
	TexCoord = WaterUV;
	RayOrigin = vec3(position_world);
}