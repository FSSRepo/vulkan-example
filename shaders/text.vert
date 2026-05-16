#version 450

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inColor;

layout(push_constant) uniform PushConstants {
    vec2 position;
    vec2 scale;
    vec2 offset;
} pc;

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec3 vColor;

void main() {
    // Add vertex offset to base position from push constants
    vec2 pos = pc.position + inPos * pc.scale;
    pos.y = -pos.y; // Flip Y for Vulkan coordinate system
    gl_Position = vec4(pos, 0.0, 1.0);
    vUV = vec2(inUV.x, inUV.y);
    vColor = inColor;
}
