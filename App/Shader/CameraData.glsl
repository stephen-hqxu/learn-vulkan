#ifndef _CAMERA_DATA_GLSL_
#define _CAMERA_DATA_GLSL_

layout(std430, set = 0, binding = 0) readonly restrict buffer CameraData {
	mat4 View, ProjectionView,
		//inverse(mat4(mat3(V))) * inverse(P)
		InvProjectionViewRotation;
	vec3 Position;

	//far * near, far - near, far
	vec3 LinearDepthFactor;
} Camera;

float lineariseDepth(const float depth) {
	return Camera.LinearDepthFactor.x / (Camera.LinearDepthFactor.z - (1.0f - depth) * Camera.LinearDepthFactor.y);
}

#endif//_CAMERA_DATA_GLSL_