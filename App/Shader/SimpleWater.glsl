#ifndef _SIMPLE_WATER_GLSL_
#define _SIMPLE_WATER_GLSL_

//all positions are defined in world space
#define WATER_RAY_PROPERTY(QUAL) layout(location = 0) QUAL RayProperty { \
	vec2 TexCoord; \
	vec3 RayOrigin; \
}

layout(std430, set = 1, binding = 0) readonly restrict buffer WaterData {
	mat4 Model;

	vec3 WaterTint;
	float IoR, DepthOfInvisibility,
		FresnelScale, AltitudeOffset, TransparencyDepth, NormalScale, NormalStrength, DistortionStrength;
} Water;

#endif//_SIMPLE_WATER_GLSL_