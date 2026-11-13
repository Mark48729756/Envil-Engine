#version 450
// 3x3 tent upsample, blended additively onto the larger mip (pipeline blend: src.a * src + dst, alpha = 1)
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 oColor;
layout(set = 1, binding = 0) uniform sampler2D uSrc;
layout(push_constant) uniform PC { vec2 texel; } pc;
void main() {
    vec2 t = pc.texel;
    vec3 r = texture(uSrc, vUV + vec2(-1, -1) * t).rgb * 1.0
           + texture(uSrc, vUV + vec2( 0, -1) * t).rgb * 2.0
           + texture(uSrc, vUV + vec2( 1, -1) * t).rgb * 1.0
           + texture(uSrc, vUV + vec2(-1,  0) * t).rgb * 2.0
           + texture(uSrc, vUV).rgb * 4.0
           + texture(uSrc, vUV + vec2( 1,  0) * t).rgb * 2.0
           + texture(uSrc, vUV + vec2(-1,  1) * t).rgb * 1.0
           + texture(uSrc, vUV + vec2( 0,  1) * t).rgb * 2.0
           + texture(uSrc, vUV + vec2( 1,  1) * t).rgb * 1.0;
    oColor = vec4(r / 16.0, 1.0);
}
