#version 450

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 pos;
} ubo;


layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragUV;
layout(location = 2) out vec3 fragTangent;
layout(location = 3) out vec3 fragBitangent;
layout(location = 4) out vec3 fragNormal;
layout(location = 5) out vec3 fragWorldPos;

void main() {
    vec4 worldPos = ubo.model * vec4(inPos, 1.0);
    gl_Position = ubo.proj * ubo.view * worldPos;
    fragWorldPos = worldPos.xyz;
    fragColor = inColor;
    fragUV = inUV;

    vec3 T = normalize(inTangent);
    T = normalize(T - dot(T, inNormal) * inNormal);
    vec3 B = cross(inNormal, T);
    fragTangent   = T;
    fragBitangent = B;
    fragNormal = inNormal;
}
