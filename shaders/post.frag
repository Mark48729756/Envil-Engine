#version 450 core
in vec2 vUV;
out vec4 oColor;
layout(binding = 0) uniform sampler2D uHdr;
layout(binding = 1) uniform sampler2D uBloom;
layout(binding = 2) uniform sampler2D uDepth;
layout(binding = 3) uniform sampler2D uHistoryColor;
layout(binding = 4) uniform sampler2D uHistoryDepth;
uniform float uExposure;
uniform float uBloomStrength;
uniform vec2 uInvResolution;
uniform int uFXAAEnabled;
uniform int uTAAEnabled;
uniform float uTAAHistoryWeight;
uniform int uMotionBlurEnabled;
uniform float uMotionBlurStrength;
uniform mat4 uInvViewProj;
uniform mat4 uPreviousViewProj;

vec3 aces(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}
float luma(vec3 c) { return dot(c,vec3(0.299,0.587,0.114)); }
vec3 fxaa(vec2 uv) {
    vec3 m=texture(uHdr,uv).rgb;
    vec3 n=texture(uHdr,uv+vec2(0.0,uInvResolution.y)).rgb;
    vec3 s=texture(uHdr,uv-vec2(0.0,uInvResolution.y)).rgb;
    vec3 e=texture(uHdr,uv+vec2(uInvResolution.x,0.0)).rgb;
    vec3 w=texture(uHdr,uv-vec2(uInvResolution.x,0.0)).rgb;
    float lo=min(luma(m),min(min(luma(n),luma(s)),min(luma(e),luma(w))));
    float hi=max(luma(m),max(max(luma(n),luma(s)),max(luma(e),luma(w))));
    if(hi-lo<max(0.0312,hi*0.125)) return m;
    return (m*2.0+n+s+e+w)/6.0;
}

// Camera-motion blur from depth reprojection. This is a first temporal pass:
// moving objects need per-object previous transforms / a velocity buffer.
vec3 cameraMotionBlur(vec2 uv, vec3 center) {
    if (uMotionBlurEnabled == 0 || uMotionBlurStrength <= 0.001) return center;

    float depth = texture(uDepth, uv).r;
    if (depth >= 0.999999) return center;

    vec4 worldH = uInvViewProj * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    if (abs(worldH.w) < 1e-6) return center;
    vec3 worldPos = worldH.xyz / worldH.w;

    vec4 oldClip = uPreviousViewProj * vec4(worldPos, 1.0);
    if (oldClip.w <= 1e-5) return center;
    vec2 oldUV = oldClip.xy / oldClip.w * 0.5 + 0.5;
    vec2 velocity = (uv - oldUV) * clamp(uMotionBlurStrength, 0.0, 1.0);

    // Bound the blur to avoid streaks from camera cuts or disocclusion.
    float maxPixels = 24.0;
    float pixelLength = length(velocity / uInvResolution);
    if (pixelLength > maxPixels) velocity *= maxPixels / pixelLength;
    if (length(velocity) < 0.00015) return center;

    vec3 accum = center * 0.20;
    float total = 0.20;
    const int taps = 8;
    for (int i = 1; i <= taps; ++i) {
        float t = float(i) / float(taps + 1);
        float weight = 1.0 - t * 0.55;
        vec2 sampleUV = uv - velocity * t;
        if (any(lessThan(sampleUV, vec2(0.001))) ||
            any(greaterThan(sampleUV, vec2(0.999)))) continue;
        accum += texture(uHdr, sampleUV).rgb * weight;
        total += weight;
    }
    return accum / max(total, 1e-4);
}


vec3 temporalResolve(vec2 uv, vec3 currentColor) {
    if (uTAAEnabled == 0) return currentColor;
    float depth = texture(uDepth, uv).r;
    // Sky has no stable world position in this depth-only reprojection path.
    if (depth >= 0.999999) return currentColor;
    vec4 worldH = uInvViewProj * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    if (abs(worldH.w) < 1e-6) return currentColor;
    vec3 worldPos = worldH.xyz / worldH.w;
    vec4 oldClip = uPreviousViewProj * vec4(worldPos, 1.0);
    if (oldClip.w <= 1e-5) return currentColor;
    vec3 oldNdc = oldClip.xyz / oldClip.w;
    vec2 oldUV = oldNdc.xy * 0.5 + 0.5;
    if (any(lessThan(oldUV, vec2(0.001))) || any(greaterThan(oldUV, vec2(0.999)))) return currentColor;
    float expectedOldDepth = oldNdc.z * 0.5 + 0.5;
    float sampledOldDepth = texture(uHistoryDepth, oldUV).r;
    float depthTolerance = max(0.0015, abs(expectedOldDepth) * 0.012);
    if (sampledOldDepth >= 0.999999 || abs(sampledOldDepth - expectedOldDepth) > depthTolerance) return currentColor;

    // Neighborhood clipping prevents stale history from leaking across disocclusions and sharp edges.
    vec3 lo = currentColor;
    vec3 hi = currentColor;
    for (int y = -1; y <= 1; ++y) for (int x = -1; x <= 1; ++x) {
        vec3 c = texture(uHdr, uv + vec2(x, y) * uInvResolution).rgb;
        c += texture(uBloom, uv + vec2(x, y) * uInvResolution).rgb * uBloomStrength;
        c = pow(aces(c * uExposure), vec3(1.0 / 2.2));
        lo = min(lo, c); hi = max(hi, c);
    }
    vec3 history = texture(uHistoryColor, oldUV).rgb;
    history = clamp(history, lo - vec3(0.025), hi + vec3(0.025));
    float weight = clamp(uTAAHistoryWeight, 0.0, 0.98);
    return mix(currentColor, history, weight);
}

void main() {
    vec3 c = uFXAAEnabled != 0 ? fxaa(vUV) : texture(uHdr, vUV).rgb;
    c = cameraMotionBlur(vUV, c);
    c += texture(uBloom, vUV).rgb * uBloomStrength;
    c = aces(c * uExposure);
    c = pow(c, vec3(1.0 / 2.2));
    c = temporalResolve(vUV, c);
    oColor = vec4(c, 1.0);
}
