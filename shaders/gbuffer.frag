#version 450 core
layout(location = 0) out vec4 oAlbedo;
layout(location = 1) out vec4 oNormal;
layout(location = 2) out vec4 oMaterial;

in vec3 vNormal;
in vec3 vWorldPos;
in vec2 vUV;

uniform vec3 uAlbedo;
uniform float uRoughness;
uniform float uMetallic;
uniform int uHasAlbedoMap;
layout(binding = 5) uniform sampler2D uAlbedoMap;
layout(binding = 6) uniform sampler2D uNormalMap;
layout(binding = 7) uniform sampler2D uRoughnessMap;
layout(binding = 8) uniform sampler2D uMetallicMap;
layout(binding = 9) uniform sampler2D uAOMap;
uniform int uHasNormalMap;
uniform int uHasRoughnessMap;
uniform int uHasMetallicMap;
uniform int uHasAOMap;

void main() {
    // simple checker on UV so large surfaces show scale/motion
    vec2 c = floor(vUV * 2.0);
    float checker = mod(c.x + c.y, 2.0) * 0.12 + 0.88;
    vec3 baseColor = uAlbedo * checker;
    if (uHasAlbedoMap != 0) baseColor *= texture(uAlbedoMap, vUV).rgb;
    vec3 N=normalize(vNormal);
    vec3 dp1=dFdx(vWorldPos), dp2=dFdy(vWorldPos);
    vec2 duv1=dFdx(vUV), duv2=dFdy(vUV);
    vec3 T=normalize(dp1*duv2.y-dp2*duv1.y); T=normalize(T-N*dot(N,T));
    vec3 B=normalize(cross(N,T))*sign(duv1.x*duv2.y-duv1.y*duv2.x);
    if(uHasNormalMap!=0){ vec3 nm=normalize(mat3(T,B,N)*(texture(uNormalMap,vUV).xyz*2.0-1.0)); if(!any(isnan(nm))) N=nm; }
    float rough=uHasRoughnessMap!=0?texture(uRoughnessMap,vUV).r:uRoughness;
    float metal=uHasMetallicMap!=0?texture(uMetallicMap,vUV).r:uMetallic;
    float ao=uHasAOMap!=0?texture(uAOMap,vUV).r:1.0;
    oAlbedo = vec4(baseColor, 1.0);
    oNormal = vec4(N, 1.0);
    oMaterial = vec4(clamp(rough,0.04,1.0),clamp(metal,0.0,1.0),ao,1.0);
}
