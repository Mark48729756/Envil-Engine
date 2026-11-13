#version 450
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

layout(set = 0, binding = 0, std140) uniform Frame { mat4 viewProj; } f;
layout(push_constant) uniform PC { mat4 model; vec4 albedoRough; vec4 metalMap; } pc;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec2 vUV;

void main() {
    vNormal = mat3(transpose(inverse(pc.model))) * aNormal;
    vUV = aUV;
    gl_Position = f.viewProj * pc.model * vec4(aPos, 1.0);
}
