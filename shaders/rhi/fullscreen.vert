#version 450
// Fullscreen triangle. Vulkan viewport is flipped (RHI), texture row 0 = top, so V is flipped relative to GL.
layout(location = 0) out vec2 vUV;
void main() {
    vec2 p = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vUV = vec2(p.x, 1.0 - p.y);
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
