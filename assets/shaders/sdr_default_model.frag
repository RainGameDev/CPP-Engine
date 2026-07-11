#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragUV;
layout(location = 2) in vec3 fragTangent;
layout(location = 3) in vec3 fragBitangent;
layout(location = 4) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D albedoMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D metallicRoughnessMap;

layout(push_constant) uniform MaterialParams {
    vec4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
} material;

void main() {
    mat3 TBN     = mat3(normalize(fragTangent), normalize(fragBitangent), normalize(fragNormal));
    vec3 texN    = texture(normalMap, fragUV).rgb * 2.0 - 1.0;
    texN.xy     *= 2.0;
    vec3 N       = normalize(TBN * texN);
    vec3 L       = normalize(vec3(1.0, 1.0, 1.0));
    float diff   = max(dot(N, L), 0.0);
    float ambient = 0.15;
    vec4 albedo  = texture(albedoMap, fragUV) * material.baseColorFactor;
    outColor     = vec4(albedo.rgb * (ambient + diff), albedo.a);
}
