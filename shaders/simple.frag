#version 450

layout(location = 0) in vec3 fragColorIn;

layout(location = 0) out vec4 fragColor;

void main() {
    fragColor = vec4(fragColorIn, 1.0);
}
