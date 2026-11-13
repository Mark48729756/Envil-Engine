#version 450
layout(location=0) in vec3 vRayDirection;
layout(location=0) out vec4 outCloud;
layout(set=0,binding=0) uniform sampler3D uCloudNoise;
layout(set=0,binding=1) uniform sampler2D uSceneColor;
layout(push_constant) uniform CloudParams {
    vec4 cameraAndTime;
    vec4 windAndCoverage;
    vec4 layerAndDensity;
    vec4 sunDirectionAndPhase;
} pc;
void main() {
    vec3 ray=normalize(vRayDirection);
    vec3 origin=pc.cameraAndTime.xyz;
    float t0=max(0.0,(pc.layerAndDensity.x-origin.y)/max(0.001,ray.y));
    float t1=max(t0,(pc.layerAndDensity.x+pc.layerAndDensity.y-origin.y)/max(0.001,ray.y));
    if(ray.y<=0.001 || t1<=t0){outCloud=vec4(0.0);return;}
    vec3 wind=vec3(pc.windAndCoverage.x,0.0,pc.windAndCoverage.y)*pc.cameraAndTime.w;
    vec3 stepRay=ray*((t1-t0)/48.0);
    vec3 p=origin+ray*t0+wind;
    float transmittance=1.0; vec3 radiance=vec3(0.0);
    for(int i=0;i<48;++i){
        vec3 uvw=p*0.00035+vec3(0.0,pc.cameraAndTime.w*0.00002,0.0);
        float noise=texture(uCloudNoise,uvw).r;
        float density=clamp((noise-(1.0-pc.windAndCoverage.z))*pc.layerAndDensity.z,0.0,1.0);
        float sun=max(0.0,dot(normalize(pc.sunDirectionAndPhase.xyz),vec3(0.0,1.0,0.0)));
        vec3 lighting=mix(vec3(0.48,0.55,0.68),vec3(1.0,0.92,0.78),sun);
        float absorb=exp(-density*0.055);
        radiance+=transmittance*(1.0-absorb)*lighting;
        transmittance*=absorb; p+=stepRay;
        if(transmittance<0.015)break;
    }
    outCloud=vec4(radiance,1.0-transmittance);
}
