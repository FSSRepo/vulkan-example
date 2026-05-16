#version 450

struct Particle {
    vec4 pos;   // xyz = position, w = life
    vec4 vel;   // xyz = velocity, w = max life
    vec4 color; // rgb = color, w = unused
};

layout(std430, binding = 0) readonly buffer Particles {
    Particle particles[];
};

layout(binding = 2) uniform Camera {
    mat4 viewProj;
} camera;

layout(location = 0) out vec3 vColor;
layout(location = 1) out float vLifeRatio;

void main() {
    uint idx = gl_VertexIndex;
    Particle p = particles[idx];
    gl_Position = camera.viewProj * vec4(p.pos.xyz, 1.0);

    float life = p.pos.w;
    float maxLife = p.vel.w;
    vLifeRatio = clamp(life / maxLife, 0.0, 1.0);
    vColor = p.color.rgb;

    gl_PointSize = mix(2.0, 8.0, vLifeRatio);
}
