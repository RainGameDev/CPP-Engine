#version 450

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 pos;
} ubo;

layout(set = 0, binding = 1) uniform TransformBufferObject {
    vec4 pos;
    vec4 rotation;
    vec4 scale;
} transform;

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

mat4 buildModel() {
    vec3 p = transform.pos.xyz;
    vec3 r = transform.rotation.xyz;
    vec3 s = transform.scale.xyz;

    float cx = cos(r.x); float sx = sin(r.x);
    float cy = cos(r.y); float sy = sin(r.y);
    float cz = cos(r.z); float sz = sin(r.z);

    mat4 rotX = mat4(
        1, 0, 0, 0,
        0, cx, -sx, 0,
        0, sx, cx, 0,
        0, 0, 0, 1);

    mat4 rotY = mat4(
        cy, 0, sy, 0,
        0, 1, 0, 0,
        -sy, 0, cy, 0,
        0, 0, 0, 1);

    mat4 rotZ = mat4(
        cz, -sz, 0, 0,
        sz, cz, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1);

    mat4 model = mat4(
        s.x, 0, 0, 0,
        0, s.y, 0, 0,
        0, 0, s.z, 0,
        0, 0, 0, 1);

    model = rotZ * rotY * rotX * model;
    model[3] = vec4(p, 1.0);
    return model;
}

void main() {
    mat4 model = buildModel();
    vec4 worldPos = model * vec4(inPos, 1.0);
    gl_Position = ubo.proj * ubo.view * worldPos;
    fragWorldPos = worldPos.xyz;
    fragColor = inColor;
    fragUV = inUV;

    mat3 normalMatrix = mat3(transpose(inverse(model)));
    vec3 T = normalize(normalMatrix * inTangent);
    vec3 N = normalize(normalMatrix * inNormal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    fragTangent   = T;
    fragBitangent = B;
    fragNormal = N;
}
