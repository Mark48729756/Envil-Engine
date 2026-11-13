#version 450
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec3 aColor;
layout(set = 0, binding = 0) uniform Tint { vec4 tint; } u;
layout(push_constant) uniform PC { float angle; } pc;
layout(location = 0) out vec3 vColor;
void main() {
    float c = cos(pc.angle), s = sin(pc.angle);
    gl_Position = vec4(mat2(c, s, -s, c) * aPos, 0.0, 1.0);
    vColor = aColor * u.tint.rgb;
}
