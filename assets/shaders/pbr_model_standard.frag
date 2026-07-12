
#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragUV;
layout(location = 2) in vec3 fragTangent;
layout(location = 3) in vec3 fragBitangent;
layout(location = 4) in vec3 fragNormal;
layout(location = 5) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D albedoMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D rmaosMap;
layout(set = 1, binding = 3) uniform sampler2D heightMap;

layout(push_constant) uniform MaterialParams {
    vec4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    float parallaxStrength;
} material;


layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 pos;
} ubo;

layout(set = 0, binding = 1) uniform TransformBufferObject {
    vec4 pos;
    vec4 rotation;
    vec4 scale;
} transformUBO;

struct LightData {
    vec4 positionOrDirection;
    vec4 colorAndIntensity;
    vec4 params;
};

layout(set = 0, binding = 2) uniform LightsBuffer {
    LightData lights[16];
    uint count;
} lightsUBO;



const float PI = 3.1415;

float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

float geometrySchlickGGX(float NdotV, float roughness) {
  float r = roughness + 1.0;
  float k = (r * r) / 8.0;
  return NdotV / (NdotV * (1.0 - k) + k);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
  return geometrySchlickGGX(max(dot(N, V), 0.0), roughness) *
    geometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
  return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}


vec2 parallaxOcclusionMapping(vec2 texCoords, vec3 viewDir, vec2 dx, vec2 dy) {
    const float minLayers = 8.0;
    const float maxLayers = 32.0;
    float numLayers = mix(maxLayers, minLayers, max(dot(vec3(0.0, 0.0, 1.0), viewDir), 0.0));

    vec2 edgeDist = min(texCoords, 1.0 - texCoords);
    float edgeFade = smoothstep(0.0, 0.5, min(edgeDist.x, edgeDist.y));

    float layerDepth = 1.0 / numLayers;
    float currentLayerDepth = 0.0;
    float viewDirZ = mix(0.15, 1.0, smoothstep(0.0, 0.3, viewDir.z));
    vec2 deltaTexCoords = viewDir.xy / viewDirZ * material.parallaxStrength * edgeFade / numLayers;

    vec2 currentTexCoords = texCoords;
    float currentDepthMapValue = textureGrad(heightMap, clamp(currentTexCoords, 0.0, 1.0), dx, dy).r;

    while (currentLayerDepth < currentDepthMapValue) {
        currentTexCoords -= deltaTexCoords;
        currentDepthMapValue = textureGrad(heightMap, clamp(currentTexCoords, 0.0, 1.0), dx, dy).r;
        currentLayerDepth += layerDepth;
    }

    vec2 prevTexCoords = currentTexCoords + deltaTexCoords;
    float afterDepth = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = textureGrad(heightMap, clamp(prevTexCoords, 0.0, 1.0), dx, dy).r - (currentLayerDepth - layerDepth);
    float weight = clamp(afterDepth / (afterDepth - beforeDepth), 0.0, 1.0);
    return clamp(mix(currentTexCoords, prevTexCoords, weight), 0.0, 1.0);
}

void main() {
  // Parallax offset
  mat3 TBN = mat3(normalize(fragTangent), normalize(fragBitangent), normalize(fragNormal));
  vec3 viewDirTangent = normalize(transpose(TBN) * (ubo.pos.xyz - fragWorldPos));
  vec2 dx = dFdx(fragUV);
  vec2 dy = dFdy(fragUV);
  vec2 uv = parallaxOcclusionMapping(fragUV, viewDirTangent, dx, dy);

  // Sample textures
  vec4 albedo = textureGrad(albedoMap, uv, dx, dy) * material.baseColorFactor;
  vec3 rmaos  = textureGrad(rmaosMap, uv, dx, dy).rgb;

  // RMAOS R=roughness, G=metallic, B=AO
  float roughness = clamp(rmaos.r * material.roughnessFactor, 0.04, 1.0);
  float metallic  = clamp(rmaos.g * material.metallicFactor, 0.0, 1.0);
  float ao        = rmaos.b;

  // Normal mapping
  vec3 texN = textureGrad(normalMap, uv, dx, dy).rgb * 2.0 - 1.0;
  vec3 N = normalize(TBN * texN);

  // Lighting
  vec3 camPos = ubo.pos.xyz;
  vec3 V = normalize(camPos - fragWorldPos);
  vec3 F0 = mix(vec3(0.04), albedo.rgb, metallic);
  vec3 Lo = vec3(0.0);

  for (uint i = 0u; i < lightsUBO.count; i++) {
    LightData light = lightsUBO.lights[i];
    vec3 lightColor = light.colorAndIntensity.rgb;
    float intensity = light.colorAndIntensity.a;
    vec3 L;

    if (light.positionOrDirection.w < 0.5) {
      L = normalize(light.positionOrDirection.xyz);
    } else {
      vec3 toLight = light.positionOrDirection.xyz - fragWorldPos;
      L = normalize(toLight);
      float dist = length(toLight);
      float radius = light.params.x;
      intensity *= 1.0 / (1.0 + dist * dist / (radius * radius));
    }

    vec3 H = normalize(V + L);

    float NDF = distributionGGX(N, H, roughness);
    float G   = geometrySmith(N, V, L, roughness);
    vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator   = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular     = numerator / denominator;

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    float NdotL = max(dot(N, L), 0.0);
    Lo += (kD * albedo.rgb / PI + specular) * lightColor * intensity * NdotL;
  }

  vec3 ambient = vec3(0.03) * albedo.rgb * ao;
  outColor = vec4(ambient + Lo, albedo.a);
}
