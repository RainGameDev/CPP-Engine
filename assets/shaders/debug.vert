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
    gl_Position = ubo.proj * ubo.view * model * vec4(inPos, 1.0);
}
