#version 450
layout(location=0) in vec3 vWorldPosition;
layout(location=1) in vec3 vWorldNormal;
layout(location=0) out vec4 outColor;
layout(set=0,binding=0) uniform sampler2D uSceneColor;
layout(set=0,binding=1) uniform sampler2D uSceneDepth;
layout(set=0,binding=2) uniform samplerCube uEnvironment;
layout(push_constant) uniform WaterParams { vec4 cameraAndTime; vec4 waterColorAndOpacity; vec4 viewportAndFoam; } pc;
void main() {
    vec3 N=normalize(vWorldNormal);
    vec3 V=normalize(pc.cameraAndTime.xyz-vWorldPosition);
    float fresnel=0.02+0.98*pow(1.0-clamp(dot(N,V),0.0,1.0),5.0);
    vec2 uv=gl_FragCoord.xy/pc.viewportAndFoam.xy;
    vec2 distortion=N.xz*0.012;
    vec3 refracted=texture(uSceneColor,clamp(uv+distortion,vec2(0.001),vec2(0.999))).rgb;
    vec3 reflected=texture(uEnvironment,reflect(-V,N)).rgb;
    float depth=texture(uSceneDepth,uv).r;
    float foam=1.0-smoothstep(0.0, max(0.001,pc.viewportAndFoam.z), depth);
    vec3 water=mix(refracted,reflected,fresnel);
    water=mix(water,vec3(0.88,0.94,0.96),clamp(foam*pc.viewportAndFoam.w,0.0,1.0));
    outColor=vec4(mix(water,pc.waterColorAndOpacity.rgb,pc.waterColorAndOpacity.a),1.0);
}
