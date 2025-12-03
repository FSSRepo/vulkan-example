#version 450

layout(set = 0, binding = 0) uniform sampler2D texSampler;

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

void main() {
    vec4 c = texture(texSampler, vUV);
    float contrast = 1.4;
    vec3 r = clamp(((c.rgb - 0.5) * contrast + 0.5), 0.0, 1.0);
    outColor = vec4(r, c.a);
}
