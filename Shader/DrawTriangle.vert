#version 460 core

#include "CameraData.glsl"

layout(location = 0) in vec3 Position;
layout(location = 1) in vec2 TexCoord;

layout(location = 0) out vec2 FragUV;

layout(std430, set = 1, binding = 1) readonly restrict buffer InstanceOffset {
    float VerticalOffset;
    //control rotation
    float Radius, Angle;
};

layout(std430, push_constant) readonly restrict uniform TriangleTransform {
    mat4 Model;
};

mat2 rotation(const float theta) {
    const float cosT = cos(theta),
        sinT = sin(theta);
    return mat2(
        cosT, -sinT,
        sinT, cosT
    );
}

void main() {
    vec4 instance_position = Model * vec4(Position, 1.0f);
    instance_position.x += Radius;
    instance_position.y += gl_InstanceIndex * VerticalOffset;
    instance_position.xz = rotation(gl_InstanceIndex * Angle) * instance_position.xz;

    gl_Position = Camera.ProjectionView * instance_position;
    FragUV = TexCoord;
}