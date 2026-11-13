#version 450
layout(location = 0) out vec4 oAlbedo;
layout(location = 1) out vec4 oNormal;
layout(location = 2) out vec4 oMaterial;

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vUV;

layout(push_constant) uniform PC { mat4 model; vec4 albedoRough; vec4 metalMap; } pc;   // metalMap: x = metallic, y = has albedo map
layout(set = 1, binding = 5) uniform sampler2D uAlbedoMap;

void main() {
    vec2 c = floor(vUV * 2.0);
    float checker = mod(c.x + c.y, 2.0) * 0.12 + 0.88;
    vec3 baseColor = pc.albedoRough.rgb * checker;
    if (pc.metalMap.y > 0.5) baseColor *= texture(uAlbedoMap, vUV).rgb;
    oAlbedo = vec4(baseColor, 1.0);
    oNormal = vec4(normalize(vNormal), 1.0);
    oMaterial = vec4(pc.albedoRough.a, pc.metalMap.x, 1.0, 1.0);
}
