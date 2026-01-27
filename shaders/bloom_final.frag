#version 450

layout(set = 0, binding = 0) uniform sampler2D sceneSampler;
layout(set = 0, binding = 1) uniform sampler2D bloomSampler;

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 sceneColor = texture(sceneSampler, vUV).rgb;
    vec3 bloomColor = texture(bloomSampler, vUV).rgb;
    
    // Additive blending
    vec3 result = sceneColor + bloomColor;
    
    // Tone mapping (optional, but good for bloom)
    result = vec3(1.0) - exp(-result * 1.5);
    
    outColor = vec4(result, 1.0);
}
