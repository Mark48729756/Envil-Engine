#version 450
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 oColor;
layout(set = 1, binding = 0) uniform sampler2D uHdr;
layout(set = 1, binding = 1) uniform sampler2D uBloom;
layout(push_constant) uniform PC { vec2 invRes; float exposure; float bloomStrength; int fxaa; } pc;

vec3 aces(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}
float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }
vec3 fxaa(vec2 uv) {
    vec3 m = texture(uHdr, uv).rgb;
    vec3 n = texture(uHdr, uv + vec2(0.0, pc.invRes.y)).rgb;
    vec3 s = texture(uHdr, uv - vec2(0.0, pc.invRes.y)).rgb;
    vec3 e = texture(uHdr, uv + vec2(pc.invRes.x, 0.0)).rgb;
    vec3 w = texture(uHdr, uv - vec2(pc.invRes.x, 0.0)).rgb;
    float lo = min(luma(m), min(min(luma(n), luma(s)), min(luma(e), luma(w))));
    float hi = max(luma(m), max(max(luma(n), luma(s)), max(luma(e), luma(w))));
    if (hi - lo < max(0.0312, hi * 0.125)) return m;
    return (m * 2.0 + n + s + e + w) / 6.0;
}
void main() {
    vec3 c = pc.fxaa != 0 ? fxaa(vUV) : texture(uHdr, vUV).rgb;
    c += texture(uBloom, vUV).rgb * pc.bloomStrength;
    c = aces(c * pc.exposure);
    oColor = vec4(pow(c, vec3(1.0 / 2.2)), 1.0);
}
