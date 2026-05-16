#version 450

struct Light {
    vec4 position;
    vec4 color;
};

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(set = 0, binding = 1) uniform Props {
    mat4 ProjView;
    vec4 cameraPos;
    Light lights[3];
} props;

layout (set = 0, binding = 2) uniform ModelProps {
    mat4 model;
} mprops;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec2 vUV;
layout(location = 2) out vec3 vPos;

void main() {
    vec4 worldPos = mprops.model * vec4(inPos, 1.0);
    gl_Position = props.ProjView * worldPos;
    vNormal = mat3(transpose(inverse(mprops.model))) * inNormal;
    vUV = inUV;
    vPos = worldPos.xyz;
}
