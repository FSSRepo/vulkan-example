#version 450

layout(set = 0, binding = 0) uniform sampler2D texSampler;

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    vec2 direction;
} pc;

void main() {
    // Pesos optimizados
    float weight[4] = float[](0.221, 0.199, 0.140, 0.081);
    
    // Usamos el tamaño de la textura para calcular el offset exacto del píxel
    vec2 tex_size = textureSize(texSampler, 0);
    vec2 tex_offset = 1.0 / tex_size;
    
    // Muestreo central
    vec3 result = texture(texSampler, vUV).rgb * weight[0];
    
    // Muestreo simétrico
    for (int i = 1; i < 4; ++i) {
        // Añadimos un pequeño offset de 0.5 para muestrear exactamente en el centro del píxel
        // Esto ayuda a evitar los artefactos de líneas verticales (jittering)
        vec2 offset = pc.direction * tex_offset * float(i);
        result += texture(texSampler, vUV + offset).rgb * weight[i];
        result += texture(texSampler, vUV - offset).rgb * weight[i];
    }
    outColor = vec4(result, 1.0);
}
