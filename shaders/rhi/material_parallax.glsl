#ifndef ANVIL_MATERIAL_PARALLAX_GLSL
#define ANVIL_MATERIAL_PARALLAX_GLSL
float anvilParallaxOcclusion(sampler2D heightMap, vec2 uv, vec3 tangentView, float heightScale, int requestedSteps) {
    int steps=clamp(requestedSteps,4,64);
    float layerDepth=1.0/float(steps);
    vec2 delta=(tangentView.xy/max(abs(tangentView.z),0.05))*heightScale/float(steps);
    vec2 currentUv=uv;
    float rayDepth=0.0;
    float sampled=texture(heightMap,currentUv).r;
    for(int i=0;i<64 && i<steps && rayDepth<sampled;++i) {
        currentUv-=delta;
        rayDepth+=layerDepth;
        sampled=texture(heightMap,currentUv).r;
    }
    return length(currentUv-uv);
}
vec3 anvilTriplanarWeights(vec3 normal,float sharpness) {
    vec3 w=pow(abs(normal),vec3(max(sharpness,0.001)));
    return w/max(w.x+w.y+w.z,0.0001);
}
#endif
