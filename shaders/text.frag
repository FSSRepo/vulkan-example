#version 450

layout(set = 0, binding = 0) uniform sampler2D texSampler;

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec3 vColor;
layout(location = 0) out vec4 outColor;

void main() {
    vec4 texColor = texture(texSampler, vUV);
    float alpha = texColor.a;
    outColor = vec4(vColor, alpha);
}
