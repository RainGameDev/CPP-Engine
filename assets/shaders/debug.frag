#version 450

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform MaterialParams {
    vec4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    float parallaxStrength;
} material;

void main() {
    outColor = material.baseColorFactor;
}
