#version 450

struct Light {
    vec4 position;
    vec4 color;
};

layout(set = 0, binding = 0) uniform sampler2D texSampler;

layout(set = 0, binding = 1) uniform Props {
    mat4 ProjView;
    vec4 cameraPos;
    Light lights[3];
} props;

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vUV;
layout(location = 2) in vec3 vPos;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(props.cameraPos.xyz - vPos);

    vec3 base = texture(texSampler, vUV).rgb;
    vec3 ambient = 0.1 * base;

    vec3 diffAccum = vec3(0.0);
    vec3 specAccum = vec3(0.0);

    for (int i = 0; i < 3; ++i) {
        vec3 L = normalize(props.lights[i].position.xyz - vPos);
        vec3 R = reflect(-L, N);

        float diff = max(dot(N, L), 0.0);
        float spec = pow(max(dot(R, V), 0.0), 32.0);

        diffAccum += props.lights[i].color.rgb * diff;
        specAccum += props.lights[i].color.rgb * spec;
    }

    vec3 color = ambient + base * diffAccum + specAccum;
    outColor = vec4(color, 1.0);
}
