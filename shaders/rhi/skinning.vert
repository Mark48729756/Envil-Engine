#version 450
layout(location=0) in vec3 inPosition;
layout(location=1) in vec3 inNormal;
layout(location=2) in uvec4 inJoints;
layout(location=3) in vec4 inWeights;
layout(location=0) out vec3 vWorldNormal;
layout(set=0,binding=0,std430) readonly buffer BonePalette { mat4 bones[]; } palette;
layout(push_constant) uniform SkinParams { mat4 model; mat4 viewProjection; } pc;
void main(){
    vec4 w=max(inWeights,vec4(0.0)); float total=max(dot(w,vec4(1.0)),1e-6); w/=total;
    mat4 skin=palette.bones[inJoints.x]*w.x+palette.bones[inJoints.y]*w.y+palette.bones[inJoints.z]*w.z+palette.bones[inJoints.w]*w.w;
    vec4 world=pc.model*skin*vec4(inPosition,1.0);
    vWorldNormal=normalize(mat3(pc.model*skin)*inNormal);
    gl_Position=pc.viewProjection*world;
}
