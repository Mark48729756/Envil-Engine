#version 450
layout(location = 0) in vec3 aPos;
layout(set = 0, binding = 0, std140) uniform Frame { mat4 viewProj; mat4 lightVP; } f;
layout(push_constant) uniform PC { mat4 model; } pc;
void main() { gl_Position = f.lightVP * pc.model * vec4(aPos, 1.0); }
