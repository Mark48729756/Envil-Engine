#version 450
layout(location=0) in vec2 vUV;
layout(location=0) out vec4 outIrradiance;
layout(set=0,binding=0) uniform sampler2D uWorldNormal;
layout(set=0,binding=1,std430) readonly buffer SHCoefficients { vec4 coeffs[9]; } sh;
layout(push_constant) uniform GIParams { mat4 inverseViewProjection; } pc;
void main(){
    vec3 n=normalize(texture(uWorldNormal,vUV).xyz*2.0-1.0);
    float x=n.x,y=n.y,z=n.z;
    float b[9]=float[9](0.282095,0.488603*y,0.488603*z,0.488603*x,1.092548*x*y,1.092548*y*z,0.315392*(3.0*z*z-1.0),1.092548*x*z,0.546274*(x*x-y*y));
    vec3 irradiance=vec3(0.0);
    for(int i=0;i<9;++i) irradiance+=sh.coeffs[i].rgb*b[i];
    outIrradiance=vec4(max(irradiance,vec3(0.0)),1.0);
}
