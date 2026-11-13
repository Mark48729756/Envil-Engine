#version 450
// Copies the gamma-encoded LDR image to the swapchain. toLinear is set when the swapchain format is *_SRGB
// (the hardware re-encodes on write, so the gamma applied by post.frag has to be undone first).
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 oColor;
layout(set = 1, binding = 0) uniform sampler2D uSrc;
layout(push_constant) uniform PC { int toLinear; } pc;
void main() {
    vec3 c = texture(uSrc, vUV).rgb;
    if (pc.toLinear != 0) c = pow(c, vec3(2.2));
    oColor = vec4(c, 1.0);
}
