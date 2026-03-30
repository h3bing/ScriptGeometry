#version 330 core

in  vec3 vWorldPos;
in  vec3 vNormal;
in  vec2 vUV;

uniform vec4  uBaseColor;
uniform vec3  uLightDir;    // world space, normalized
uniform vec3  uCameraPos;
uniform bool  uSelected;
uniform float uRoughness;

out vec4 FragColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightDir);
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 H = normalize(L + V);

    // Blinn-Phong shading
    float ambient  = 0.20;
    float diff     = max(dot(N, L), 0.0);
    float spec     = pow(max(dot(N, H), 0.0), mix(8.0, 128.0, 1.0 - uRoughness));

    vec3 baseCol = uBaseColor.rgb;
    vec3 color   = baseCol * (ambient + diff * 0.7) + vec3(1.0) * spec * 0.15;

    if (uSelected)
        color = mix(color, vec3(1.0, 0.55, 0.05), 0.40);

    FragColor = vec4(color, uBaseColor.a);
}
