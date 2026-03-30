#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aTangent;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec2 aUV;

uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat3 uNormalMat;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;

void main() {
    vWorldPos   = vec3(uModel * vec4(aPos, 1.0));
    vNormal     = normalize(uNormalMat * aNormal);
    vUV         = aUV;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
