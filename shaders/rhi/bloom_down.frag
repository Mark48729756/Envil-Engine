#version 450
// 13-tap downsample (Call of Duty: Advanced Warfare). First level also does a soft-knee brightness threshold.
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 oColor;
layout(set = 1, binding = 0) uniform sampler2D uSrc;
layout(push_constant) uniform PC { vec2 texel; int first; float threshold; } pc;

vec3 prefilter(vec3 c) {
    if (pc.first == 0) return c;
    float br = max(c.r, max(c.g, c.b));
    float k = pc.threshold * 0.5;
    float soft = clamp(br - pc.threshold + k, 0.0, 2.0 * k);
    soft = soft * soft / (4.0 * k + 1e-4);
    float contrib = max(soft, br - pc.threshold) / max(br, 1e-4);
    return c * contrib;
}
vec3 s(vec2 o) { return prefilter(texture(uSrc, vUV + o * pc.texel).rgb); }

void main() {
    vec3 a = s(vec2(-2, -2)), b = s(vec2(0, -2)), c = s(vec2(2, -2));
    vec3 d = s(vec2(-2,  0)), e = s(vec2(0,  0)), f = s(vec2(2,  0));
    vec3 g = s(vec2(-2,  2)), h = s(vec2(0,  2)), i = s(vec2(2,  2));
    vec3 j = s(vec2(-1, -1)), k = s(vec2(1, -1));
    vec3 l = s(vec2(-1,  1)), m = s(vec2(1,  1));
    vec3 r = e * 0.125 + (a + c + g + i) * 0.03125 + (b + d + f + h) * 0.0625 + (j + k + l + m) * 0.125;
    oColor = vec4(r, 1.0);
}
