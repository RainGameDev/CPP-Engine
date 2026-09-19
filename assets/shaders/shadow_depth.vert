#version 450

struct ShadowData {
    mat4 viewProj;
    vec4 pos_far;
    vec4 dir_type;
};

layout(set = 0, binding = 3) uniform ShadowUBO {
    ShadowData shadows[4];
    uint count;
} shadow;

layout(push_constant) uniform ShadowPC {
    int layer;
} pc;

layout(set = 0, binding = 1) uniform TransformBufferObject {
    mat4 model;
} transform;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inNormal;

void main() {
    gl_Position = shadow.shadows[pc.layer].viewProj * transform.model * vec4(inPos, 1.0);
}
