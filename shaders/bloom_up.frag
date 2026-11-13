#version 450 core
// 3x3 tent upsample, blended additively onto the larger mip
in vec2 vUV;
out vec4 oColor;
layout(binding = 0) uniform sampler2D uSrc;
uniform vec2 uTexel;
void main() {
    vec3 r = texture(uSrc, vUV + vec2(-1, -1) * uTexel).rgb * 1.0
           + texture(uSrc, vUV + vec2( 0, -1) * uTexel).rgb * 2.0
           + texture(uSrc, vUV + vec2( 1, -1) * uTexel).rgb * 1.0
           + texture(uSrc, vUV + vec2(-1,  0) * uTexel).rgb * 2.0
           + texture(uSrc, vUV).rgb * 4.0
           + texture(uSrc, vUV + vec2( 1,  0) * uTexel).rgb * 2.0
           + texture(uSrc, vUV + vec2(-1,  1) * uTexel).rgb * 1.0
           + texture(uSrc, vUV + vec2( 0,  1) * uTexel).rgb * 2.0
           + texture(uSrc, vUV + vec2( 1,  1) * uTexel).rgb * 1.0;
    oColor = vec4(r / 16.0, 1.0);
}
