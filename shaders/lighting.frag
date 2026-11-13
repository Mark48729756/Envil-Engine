#version 450 core
#define MAX_LIGHTS 32
const float PI = 3.14159265359;

in vec2 vUV;
out vec4 oColor;

layout(binding = 0) uniform sampler2D gAlbedo;
layout(binding = 1) uniform sampler2D gNormal;
layout(binding = 2) uniform sampler2D gMaterial;
layout(binding = 3) uniform sampler2D gDepth;
layout(binding = 4) uniform sampler2DArrayShadow uShadow;
layout(binding = 13) uniform sampler2DArray uShadowRaw;
// M51 optional preconvolved environment resources; disabled by default.
layout(binding = 10) uniform samplerCube uIrradianceMap;
layout(binding = 11) uniform samplerCube uPrefilterMap;
layout(binding = 12) uniform sampler2D uBrdfLut;
uniform int uIBLReady;
uniform float uEnvironmentIntensity;

uniform mat4 uInvViewProj;
uniform mat4 uViewProj;
uniform int uSSAOEnabled;
uniform float uAORadius;
uniform float uAOStrength;
uniform vec3 uCamPos;
uniform vec3 uSunDir;
uniform vec3 uSunRadiance;
uniform int uLightCount;
uniform vec3 uLightPos[MAX_LIGHTS];
uniform vec3 uLightRadiance[MAX_LIGHTS];
uniform float uLightRadius[MAX_LIGHTS];
uniform mat4 uLightVP[3];
uniform vec3 uCascadeSplits;
uniform mat4 uCameraView;
uniform float uShadowLightSize;
uniform int uShadowsOn;
uniform float uShadowBias;
uniform float uShadowTexel;
uniform float uShadowTexelWorld[3];
uniform int uSSREnabled;
uniform float uSSRStrength;
uniform int uFogEnabled;
uniform vec3 uFogColor;
uniform float uFogDensity;
uniform float uCloudTime;

// Cascaded shadow maps with a blocker-search PCSS filter. The raw depth
// sampler estimates average blocker distance; the comparison sampler performs
// variable-radius 16-tap PCF. Cascade selection uses positive view-space depth.
float sunShadow(vec3 P, vec3 N, vec3 L) {
    if (uShadowsOn == 0) return 1.0;
    float viewDepth = -(uCameraView * vec4(P,1.0)).z;
    int cascade = viewDepth <= uCascadeSplits.x ? 0 : (viewDepth <= uCascadeSplits.y ? 1 : 2);
    if (viewDepth > uCascadeSplits.z) return 1.0;
    vec4 lp = uLightVP[cascade] * vec4(P + N * (uShadowTexelWorld[cascade] * 1.5), 1.0);
    vec3 sc = lp.xyz / lp.w * 0.5 + 0.5;
    if (sc.z > 1.0 || sc.x < 0.0 || sc.y < 0.0 || sc.x > 1.0 || sc.y > 1.0) return 1.0;
    float bias = uShadowBias * (1.0 - clamp(dot(N,L),0.0,1.0)) + 0.0002;
    const vec2 poisson[16] = vec2[](
        vec2(-0.94201624,-0.39906216), vec2(0.94558609,-0.76890725),
        vec2(-0.09418410,-0.92938870), vec2(0.34495938,0.29387760),
        vec2(-0.91588581,0.45771432), vec2(-0.81544232,-0.87912464),
        vec2(-0.38277543,0.27676845), vec2(0.97484398,0.75648379),
        vec2(0.44323325,-0.97511554), vec2(0.53742981,-0.47373420),
        vec2(-0.26496911,-0.41893023), vec2(0.79197514,0.19090188),
        vec2(-0.24188840,0.99706507), vec2(-0.81409955,0.91437590),
        vec2(0.19984126,0.78641367), vec2(0.14383161,-0.14100790));
    float angle = fract(sin(dot(gl_FragCoord.xy,vec2(12.9898,78.233)))*43758.5453)*6.2831853;
    mat2 rot = mat2(cos(angle),-sin(angle),sin(angle),cos(angle));
    vec2 texel = vec2(uShadowTexel);
    float blockerSum = 0.0; float blockers = 0.0;
    for (int i=0;i<16;++i) {
        float d = texture(uShadowRaw,vec3(sc.xy + rot*poisson[i]*texel*3.0,float(cascade))).r;
        if (d < sc.z - bias) { blockerSum += d; blockers += 1.0; }
    }
    float filterRadius = 1.5;
    if (blockers > 0.0) {
        float avgBlocker = blockerSum / blockers;
        float penumbra = max(sc.z - avgBlocker,0.0) / max(avgBlocker,0.02);
        filterRadius = clamp(penumbra * uShadowLightSize * 180.0, 1.0, 6.0);
    }
    float visibility = 0.0;
    for (int i=0;i<16;++i)
        visibility += texture(uShadow,vec4(sc.xy + rot*poisson[i]*texel*filterRadius,float(cascade),sc.z-bias));
    return visibility / 16.0;
}

// Screen-space ambient occlusion. Samples are projected from a small
// hemisphere around the receiver; depth is reconstructed into world space.
float ambientOcclusion(vec3 P, vec3 N) {
    if (uSSAOEnabled == 0) return 1.0;
    const vec3 kernel[12] = vec3[](
        vec3( 0.18, 0.12, 0.31), vec3(-0.24, 0.16, 0.28),
        vec3( 0.12,-0.32, 0.22), vec3(-0.34,-0.18, 0.20),
        vec3( 0.42, 0.08, 0.15), vec3(-0.40, 0.24, 0.12),
        vec3( 0.08, 0.48, 0.18), vec3(-0.12,-0.48, 0.16),
        vec3( 0.52,-0.22, 0.08), vec3(-0.50,-0.28, 0.10),
        vec3( 0.28, 0.42, 0.12), vec3(-0.28, 0.40, 0.14)
    );
    vec3 helper = abs(N.y) < 0.95 ? vec3(0,1,0) : vec3(1,0,0);
    vec3 T = normalize(cross(helper, N));
    vec3 B = normalize(cross(N, T));
    float occlusion = 0.0;
    float valid = 0.0;
    for (int i = 0; i < 12; ++i) {
        vec3 k = kernel[i];
        vec3 dir = normalize(T * k.x + B * k.y + N * abs(k.z));
        vec3 Q = P + dir * uAORadius;
        vec4 clip = uViewProj * vec4(Q, 1.0);
        if (clip.w <= 0.0) continue;
        vec2 uv = clip.xy / clip.w * 0.5 + 0.5;
        if (any(lessThan(uv, vec2(0.002))) || any(greaterThan(uv, vec2(0.998)))) continue;
        float d = texture(gDepth, uv).r;
        if (d >= 0.99999) continue;
        vec4 h = uInvViewProj * vec4(uv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
        vec3 R = h.xyz / h.w;
        float rangeWeight = smoothstep(0.0, 1.0, uAORadius / (length(R - P) + 1e-4));
        float blocked = dot(N, R - P) > 0.025 ? 1.0 : 0.0;
        occlusion += blocked * rangeWeight;
        valid += 1.0;
    }
    float ao = 1.0 - (occlusion / max(valid, 1.0)) * uAOStrength;
    return clamp(ao, 0.25, 1.0);
}


// Bounded screen-space reflection. It is a local approximation: off-screen and
// hidden geometry cannot contribute, so the result fades smoothly with roughness.
vec3 screenReflection(vec3 P, vec3 N, vec3 V, float roughness) {
    if (uSSREnabled == 0 || roughness > 0.72) return vec3(0.0);
    vec3 R = reflect(-V, N);
    float stepLen = mix(0.12, 0.32, roughness);
    vec3 hitColor = vec3(0.0); float confidence = 0.0;
    for (int i=1; i<=28; ++i) {
        vec3 Q = P + R * (stepLen * float(i));
        vec4 clip = uViewProj * vec4(Q, 1.0);
        if (clip.w <= 0.0) break;
        vec2 uv = clip.xy / clip.w * 0.5 + 0.5;
        if (any(lessThan(uv, vec2(0.002))) || any(greaterThan(uv, vec2(0.998)))) break;
        float sceneDepth = texture(gDepth, uv).r;
        if (sceneDepth < 0.99999) {
            vec4 h = uInvViewProj * vec4(uv*2.0-1.0, sceneDepth*2.0-1.0, 1.0);
            vec3 sceneP = h.xyz / h.w;
            float delta = length(sceneP-Q);
            if (delta < 0.22 + stepLen*float(i)*0.035 && dot(normalize(sceneP-P),N) > -0.15) {
                float edge = smoothstep(0.0,0.08,min(min(uv.x,uv.y),min(1.0-uv.x,1.0-uv.y)));
                confidence = (1.0-float(i)/30.0)*edge*(1.0-roughness/0.8);
                hitColor = texture(gAlbedo,uv).rgb;
                break;
            }
        }
    }
    return hitColor * confidence * uSSRStrength;
}

float distGGX(float NdotH, float a) {
    float a2 = a * a;
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}
float geomSchlick(float NdotX, float k) { return NdotX / (NdotX * (1.0 - k) + k); }
vec3 fresnel(float cosT, vec3 F0) { return F0 + (1.0 - F0) * pow(clamp(1.0 - cosT, 0.0, 1.0), 5.0); }

vec3 brdf(vec3 N, vec3 V, vec3 L, vec3 radiance, vec3 albedo, float rough, float metal, vec3 F0) {
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 1e-4);
    float a = rough * rough;
    float k = (rough + 1.0) * (rough + 1.0) / 8.0;
    float D = distGGX(max(dot(N, H), 0.0), a);
    float G = geomSchlick(NdotV, k) * geomSchlick(NdotL, k);
    vec3 F = fresnel(max(dot(H, V), 0.0), F0);
    vec3 spec = D * G * F / (4.0 * NdotV * NdotL + 1e-4);
    vec3 kD = (1.0 - F) * (1.0 - metal);
    return (kD * albedo / PI + spec) * radiance * NdotL;
}


// M47: view-ray atmospheric integration + true volumetric cloud ray marching.
// Coordinates are world-space; the cloud layer is intentionally expressed in engine units.
float hash31(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}
float noise3(vec3 x) {
    vec3 i = floor(x), f = fract(x);
    f = f*f*(3.0-2.0*f);
    return mix(mix(mix(hash31(i), hash31(i+vec3(1,0,0)),f.x),
                   mix(hash31(i+vec3(0,1,0)),hash31(i+vec3(1,1,0)),f.x),f.y),
               mix(mix(hash31(i+vec3(0,0,1)),hash31(i+vec3(1,0,1)),f.x),
                   mix(hash31(i+vec3(0,1,1)),hash31(i+vec3(1,1,1)),f.x),f.y),f.z);
}
// ---- M101: photoreal cumulus (Schneider-style shape + erosion, multi-scatter, dual-lobe HG) ----
const float CLOUD_BASE = 7.5;
const float CLOUD_TOP  = 14.0;
const float CLOUD_SIGMA = 1.35;
vec3 cloudHash33(vec3 p) {
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.xxy + p.yxx) * p.zyx);
}
// Cheap 2x2x2 cellular noise: 0 at feature points, ~1 between them.
float cloudWorley(vec3 p) {
    vec3 b = floor(p - 0.5);
    float d = 1.0;
    for (int k = 0; k < 8; ++k) {
        vec3 o = vec3(float(k & 1), float((k >> 1) & 1), float((k >> 2) & 1));
        vec3 c = b + o;
        vec3 r = c + cloudHash33(c) - p;
        d = min(d, dot(r, r));
    }
    return clamp(sqrt(d) * 1.15, 0.0, 1.0);
}
vec3 cloudWind() { return vec3(0.018, 0.0, 0.011) * uCloudTime; }
float cloudHeightFrac(float y) { return clamp((y - CLOUD_BASE) / (CLOUD_TOP - CLOUD_BASE), 0.0, 1.0); }
// flat, dense base; rounded billowing top
float cloudHeightGradient(float hf) {
    return smoothstep(0.0, 0.09, hf) * (1.0 - smoothstep(0.45, 1.0, hf));
}
float cloudCoverage(vec3 p) {
    float c = noise3(vec3(p.x * 0.021, 3.7, p.z * 0.021) + cloudWind() * 0.4) * 0.7 + noise3(vec3(p.x * 0.067, 8.1, p.z * 0.067) + cloudWind() * 0.6) * 0.3;
    return smoothstep(0.30, 0.72, c);
}
float cloudFbm(vec3 q) {
    return noise3(q) * 0.62 + noise3(q * 2.03 + vec3(17.1, 9.2, 13.7)) * 0.38;
}
// Returns base density 0..1 (no erosion). 'skip' set when the point is certainly empty.
float cloudBase(vec3 p, out bool skip) {
    float hf = cloudHeightFrac(p.y);
    float grad = cloudHeightGradient(hf);
    skip = true;
    if (grad <= 0.001) return 0.0;
    vec3 q = p * vec3(0.072, 0.135, 0.072) + cloudWind();
    float perl = cloudFbm(q);
    float cov = cloudCoverage(p);
    float lin = (grad - 0.5) * 0.46 + (cov - 0.5) * 0.52;
    // conservative bound: worley can lift perl by at most 0.35*(1-perl)
    if (perl + 0.35 * (1.0 - perl) + lin < 0.52) return 0.0;
    skip = false;
    float wor = 1.0 - cloudWorley(q * 1.7);
    float shape = mix(perl, wor, 0.35) + lin;
    return smoothstep(0.50, 0.80, shape);
}
float cloudErode(vec3 p, float d, float amount) {
    if (amount <= 0.0 || d <= 0.0) return d;
    vec3 q = p * vec3(0.072, 0.135, 0.072) + cloudWind();
    float w = 1.0 - cloudWorley(q * 5.3 + vec3(3.3, 1.7, 5.1));
    float n = noise3(q * 9.1 + vec3(7.7, 2.3, 4.9));
    float detail = w * 0.65 + n * 0.35;
    return clamp(d - (1.0 - d) * 0.5 * (1.0 - detail) * amount - 0.04 * amount, 0.0, 1.0);
}
float cloudDensity(vec3 p) {
    bool skip;
    float d = cloudBase(p, skip);
    if (skip) return 0.0;
    return cloudErode(p, d, 1.0);
}
float cloudDensityLite(vec3 p) {
    // light-march version: base shape only, no erosion
    bool skip;
    return cloudBase(p, skip);
}
vec3 integrateAtmosphere(vec3 origin, vec3 direction, vec3 sunDir, vec3 sunColor) {
    // Exponential Rayleigh/Mie density integration with view and sun optical depth.
    const float rayleighH=8.0, mieH=1.2;
    const vec3 betaR=vec3(0.0058,0.0135,0.0331);
    const vec3 betaM=vec3(0.0030);
    float mu=dot(direction,sunDir);
    float rayleighPhase=3.0/(16.0*PI)*(1.0+mu*mu);
    float g=0.78;
    float miePhase=(1.0-g*g)/(4.0*PI*pow(max(1.0+g*g-2.0*g*mu,0.02),1.5));
    vec3 opticalR=vec3(0.0), opticalM=vec3(0.0), scatter=vec3(0.0);
    float tMax = direction.y < -0.02 ? min(120.0, 18.0/max(abs(direction.y),0.08)) : 120.0;
    const int VIEW_STEPS=12;
    float stepLen=tMax/float(VIEW_STEPS);
    for(int i=0;i<VIEW_STEPS;++i){
        float t=(float(i)+0.5)*stepLen;
        float altitude=max(origin.y+direction.y*t,0.0);
        float dr=exp(-altitude/rayleighH), dm=exp(-altitude/mieH);
        opticalR += vec3(dr*stepLen); opticalM += vec3(dm*stepLen);
        vec3 sunOptR=vec3(0.0), sunOptM=vec3(0.0);
        for(int j=0;j<3;++j){
            float st=(float(j)+0.5)*3.0;
            float sh=max(origin.y+direction.y*t+sunDir.y*st,0.0);
            sunOptR += vec3(exp(-sh/rayleighH)*0.75);
            sunOptM += vec3(exp(-sh/mieH)*0.75);
        }
        vec3 extinction=exp(-(betaR*(opticalR+sunOptR)+betaM*(opticalM+sunOptM)));
        scatter += extinction*(betaR*dr*rayleighPhase+betaM*dm*miePhase)*stepLen;
    }
    return scatter*sunColor*18.0;
}
vec4 renderVolumetricClouds(vec3 origin, vec3 direction, vec3 sunDir, vec3 sunColor) {
    if(direction.y<=0.02) return vec4(0.0);
    float t0=(CLOUD_BASE-origin.y)/direction.y, t1=(CLOUD_TOP-origin.y)/direction.y;
    float begin=max(t0,0.0), finish=max(min(t1,160.0),0.0);
    if(finish<=begin) return vec4(0.0);
    // Adaptive step: short paths (looking up) get fine steps, long grazing paths get
    // coarse ones; IGN jitter turns banding into stable grain. Empty space is skipped 2x.
    const int CLOUD_STEPS=56;
    float ds=max((finish-begin)/float(CLOUD_STEPS),0.28);
    float jitter=fract(52.9829189*fract(dot(gl_FragCoord.xy,vec2(0.06711056,0.00583715))));
    float mu=dot(direction,sunDir);
    float sunUp=clamp(sunDir.y*1.6+0.25,0.0,1.0);
    // dual-lobe Henyey-Greenstein (relative to isotropic): silver lining + soft back scatter
    float g1=0.85,g2=-0.25;
    float hgF=(1.0-g1*g1)/pow(max(1.0+g1*g1-2.0*g1*mu,0.04),1.5);
    float hgB=(1.0-g2*g2)/pow(max(1.0+g2*g2-2.0*g2*mu,0.04),1.5);
    float phase=clamp(mix(hgF,hgB,0.32),0.25,3.2);
    vec3 skyTopAmb=vec3(0.46,0.58,0.84)*(0.25+0.75*sunUp);
    vec3 skyLowAmb=vec3(0.20,0.24,0.31)*(0.25+0.75*sunUp);
    float trans=1.0; vec3 radiance=vec3(0.0); float hitT=0.0, hitW=0.0;
    float t=begin+jitter*ds;
    for(int i=0;i<CLOUD_STEPS*2;++i){
        if(t>=finish) break;
        vec3 p=origin+direction*t;
        bool skip;
        float d=cloudBase(p,skip);
        if(skip){ t+=ds*2.0; continue; }
        // erosion detail fades out with distance (LOD): far clouds stay smooth and cheap
        float detailAmt=1.0-smoothstep(55.0,130.0,t);
        d=cloudErode(p,d,detailAmt);
        if(d>0.012){
            float sigma=d*CLOUD_SIGMA;
            // 4-tap exponential light march toward the sun
            float lightOD=0.0;
            lightOD+=cloudDensityLite(p+sunDir*0.6)*0.6;
            lightOD+=cloudDensityLite(p+sunDir*1.5)*0.9;
            lightOD+=cloudDensityLite(p+sunDir*3.0)*1.5;
            lightOD+=cloudDensityLite(p+sunDir*5.5)*2.5;
            lightOD*=CLOUD_SIGMA;
            // multi-scattering approximation: 3 octaves with weaker extinction and flatter phase
            float sunL=0.0, a=1.0, b=1.0, c=1.0;
            for(int o=0;o<3;++o){
                float ph=mix(1.0,phase,c);
                sunL+=b*exp(-lightOD*a)*ph;
                a*=0.5; b*=0.5; c*=0.5;
            }
            float powder=1.0-exp(-sigma*2.2);
            sunL*=mix(1.0,powder,0.6*(1.0-max(mu,0.0)));
            float hf=cloudHeightFrac(p.y);
            vec3 ambient=mix(skyLowAmb,skyTopAmb,hf)*mix(1.0,exp(-lightOD*0.25),0.5);
            vec3 lum=ambient+sunColor*sunL*0.72;
            float stepT=exp(-sigma*ds);
            radiance+=trans*lum*(1.0-stepT);
            hitT+=t*trans*(1.0-stepT); hitW+=trans*(1.0-stepT);
            trans*=stepT;
            if(trans<0.03) break;
        }
        t+=ds;
    }
    float alpha=1.0-trans;
    // aerial perspective: distant clouds blend into the horizon haze
    float dist=hitW>1e-4?hitT/hitW:finish;
    float aerial=1.0-exp(-dist*0.0085);
    vec3 haze=vec3(0.62,0.72,0.88)*(0.25+0.75*sunUp)*1.15;
    radiance=mix(radiance,haze*alpha,aerial*0.7);
    float fade=smoothstep(0.02,0.14,direction.y)*(1.0-0.55*aerial);
    return vec4(radiance*fade,alpha*fade);
}

// Split-sum image-based lighting using irradiance, GGX-prefiltered radiance,
// and a 2D integrated BRDF lookup. The analytic hemisphere remains the safe fallback.
vec3 evaluateIBL(vec3 N, vec3 V, vec3 albedo, float roughness, float metallic, vec3 F0, float ao) {
    vec3 R = reflect(-V, N);
    float NdotV = max(dot(N, V), 0.0);
    vec3 irradiance = texture(uIrradianceMap, N).rgb;
    vec3 diffuse = irradiance * albedo * (1.0 - metallic);
    const float MAX_REFLECTION_LOD = 5.0;
    vec3 prefiltered = textureLod(uPrefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 brdfSample = texture(uBrdfLut, vec2(NdotV, roughness)).rg;
    vec3 F = fresnel(NdotV, F0);
    vec3 specular = prefiltered * (F * brdfSample.x + brdfSample.y);
    float specularAo = clamp(pow(max(NdotV + ao, 0.0), exp2(-16.0 * roughness - 1.0)) - 1.0 + ao, 0.0, 1.0);
    return (diffuse * (1.0 - F) + specular * specularAo) * ao;
}

// M52: height fog integrates density along the camera-to-fragment segment.
// It is an analytic approximation (not a froxel/3D-volume simulation) and has
// a stable low-cost fallback suitable for all supported rendering backends.
vec2 heightFogTransmittance(vec3 cameraPosition, vec3 worldPosition, float baseDensity) {
    vec3 ray = worldPosition - cameraPosition;
    float distanceToCamera = length(ray);
    if (distanceToCamera < 1e-4 || baseDensity <= 0.0) return vec2(1.0, 0.0);
    float heightFalloff = 0.055;
    float cameraDensity = exp(-max(cameraPosition.y, 0.0) * heightFalloff);
    float fragmentDensity = exp(-max(worldPosition.y, 0.0) * heightFalloff);
    float averageDensity = (cameraDensity + fragmentDensity) * 0.5;
    float opticalDepth = min(baseDensity * averageDensity * distanceToCamera, 12.0);
    float transmittance = exp(-opticalDepth);
    return vec2(transmittance, 1.0 - transmittance);
}

void main() {
    float depth = texture(gDepth, vUV).r;
    vec3 skyTop = vec3(0.35, 0.55, 0.95), skyHorizon = vec3(0.85, 0.9, 1.0);

    vec4 ndc = vec4(vUV * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 wp = uInvViewProj * ndc;
    vec3 P = wp.xyz / wp.w;

    if (depth >= 1.0) {
        vec3 dir = normalize(P - uCamPos);
        vec3 sunDir = normalize(-uSunDir);
        vec3 sky = integrateAtmosphere(uCamPos, dir, sunDir, uSunRadiance);
        float sunDot = max(dot(dir, sunDir), 0.0);
        float solarDisk = smoothstep(0.99994, 0.999985, sunDot);
        float solarGlow = pow(sunDot, 48.0) * 0.16 + pow(sunDot, 8.0) * 0.035;
        sky += uSunRadiance * (solarDisk * 22.0 + solarGlow);
        vec4 clouds = renderVolumetricClouds(uCamPos, dir, sunDir, uSunRadiance);
        sky = mix(sky, clouds.rgb + sky*0.18, clouds.a);
        oColor = vec4(max(sky,vec3(0.0)), 1.0);
        return;
    }

    vec3 albedo = texture(gAlbedo, vUV).rgb;
    vec3 N = normalize(texture(gNormal, vUV).xyz);
    vec3 rm = texture(gMaterial, vUV).rgb;
    float rough = clamp(rm.x, 0.04, 1.0), metal = rm.y, materialAO = clamp(rm.z,0.0,1.0);
    vec3 V = normalize(uCamPos - P);
    vec3 F0 = mix(vec3(0.04), albedo, metal);

    vec3 Lsun = normalize(-uSunDir);
    vec3 Lo = brdf(N, V, Lsun, uSunRadiance, albedo, rough, metal, F0) * sunShadow(P, N, Lsun);
    for (int i = 0; i < uLightCount; ++i) {
        vec3 d = uLightPos[i] - P;
        float dist2 = max(dot(d, d), 0.01);
        float distanceToLight = sqrt(dist2);
        float range = uLightRadius[i] > 0.0 ? clamp(1.0 - pow(distanceToLight / uLightRadius[i], 4.0), 0.0, 1.0) : 1.0;
        range *= range;
        Lo += brdf(N, V, d * inversesqrt(dist2), uLightRadiance[i] * (range / dist2), albedo, rough, metal, F0);
    }

    // Energy-aware hemispherical ambient approximation. This is not IBL: the
    // environment cubemap convolution and BRDF LUT pipeline is a later milestone.
    float up = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 hemi = mix(vec3(0.045, 0.042, 0.040), vec3(0.19, 0.25, 0.36), up);
    vec3 Fambient = F0 + (max(vec3(1.0 - rough), F0) - F0) * pow(1.0 - max(dot(N, V), 0.0), 5.0);
    vec3 kAmbientDiffuse = (1.0 - Fambient) * (1.0 - metal);
    vec3 ambientDiffuse = hemi * albedo * kAmbientDiffuse;
    // Restrained roughness-aware specular fill prevents metals becoming black
    // without pretending that an environment map is present.
    vec3 ambientSpecular = hemi * Fambient * (0.10 + 0.12 * (1.0 - rough));
    float ao = ambientOcclusion(P, N) * materialAO;
    // Specular occlusion suppresses bright environment fill in creases and cavities.
    float specularAo = clamp(pow(max(dot(N, V) + ao, 0.0), exp2(-16.0 * rough - 1.0)) - 1.0 + ao, 0.0, 1.0);
    vec3 ambient = ambientDiffuse * ao + ambientSpecular * specularAo;
    if (uIBLReady != 0) ambient = evaluateIBL(N, V, albedo, rough, metal, F0, ao) * uEnvironmentIntensity;
    vec3 reflected = screenReflection(P, N, V, rough);
    vec3 color = Lo + ambient + reflected;
    if (uFogEnabled != 0) {
        vec2 fogTerms = heightFogTransmittance(uCamPos, P, max(uFogDensity, 0.0));
        color = color * fogTerms.x + uFogColor * fogTerms.y;
    }
    if (any(isnan(color)) || any(isinf(color))) color = ambient;
    oColor = vec4(max(color, vec3(0.0)), 1.0);
}
