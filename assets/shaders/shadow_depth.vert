#version 450

layout(set = 0, binding = 3) uniform ShadowUBO {
    mat4 lightViewProj;
} shadow;

layout(set = 0, binding = 1) uniform TransformBufferObject {
    mat4 model;
} transform;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inNormal;

void main() {
    gl_Position = shadow.lightViewProj * transform.model * vec4(inPos, 1.0);
}
