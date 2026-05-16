#version 450

layout(location = 0) in vec3 vColor;
layout(location = 1) in float vLifeRatio;

layout(location = 0) out vec4 outColor;

void main() {
    vec2 center = gl_PointCoord - vec2(0.5);
    float dist = length(center);
    if (dist > 0.5) discard;

    float alpha = 1.0 - smoothstep(0.0, 0.5, dist);
    alpha *= vLifeRatio;

    outColor = vec4(vColor, alpha);
}
