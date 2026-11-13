#pragma once

#include "anvil/Camera.h"
#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <algorithm>
#include <cmath>

namespace anvil {

// Renderer-independent directional-light shadow camera. Both render paths use
// exactly the same stabilized frustum; explicit APIs opt into [0, 1] clip Z.
struct ShadowFrustum {
    glm::mat4 lightVP{1.0f};
    float texelWorld = 0.0f;
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
};

inline ShadowFrustum buildShadowFrustum(const glm::vec3& cameraPosition,
                                        const glm::vec3& cameraForward,
                                        const glm::vec3& sunDirection,
                                        float radius, int mapSize,
                                        bool zeroToOneClipDepth = false) {
    radius = std::max(radius, 0.001f);
    mapSize = std::max(mapSize, 1);
    glm::vec3 dir = sunDirection;
    if (glm::dot(dir, dir) < 1.0e-12f) dir = glm::vec3(-0.4f, -1.0f, -0.3f);
    dir = glm::normalize(dir);
    const glm::vec3 up = std::abs(dir.y) > 0.99f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);

    glm::vec3 center = cameraPosition + cameraForward * (radius * 0.5f);
    const glm::mat4 rotation = glm::lookAt(glm::vec3(0.0f), dir, up);
    const float texelWorld = 2.0f * radius / static_cast<float>(mapSize);
    glm::vec3 lightCenter = glm::vec3(rotation * glm::vec4(center, 1.0f));
    lightCenter.x = std::floor(lightCenter.x / texelWorld) * texelWorld;
    lightCenter.y = std::floor(lightCenter.y / texelWorld) * texelWorld;
    center = glm::vec3(glm::inverse(rotation) * glm::vec4(lightCenter, 1.0f));

    const glm::mat4 view = glm::lookAt(center - dir * radius * 2.0f, center, up);
    glm::mat4 projection = glm::ortho(-radius, radius, -radius, radius, 0.1f, radius * 4.0f);
    if (zeroToOneClipDepth) {
        glm::mat4 clipFix(1.0f);
        clipFix[2][2] = 0.5f;
        clipFix[3][2] = 0.5f;
        projection = clipFix * projection;
    }
    return {projection * view, texelWorld, dir};
}


// Three stabilized directional-light cascades. Split positions use a practical
// logarithmic/uniform blend; each slice is fitted to its own camera-frustum corners.
struct ShadowCascadeSet {
    std::array<glm::mat4, 3> lightVP{glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f)};
    glm::vec3 splitDepths{0.0f};
    std::array<float, 3> texelWorld{};
};

inline ShadowCascadeSet buildShadowCascades(const Camera& camera, float aspect,
                                             const glm::vec3& sunDirection,
                                             float shadowDistance, int mapSize,
                                             float splitLambda = 0.65f) {
    ShadowCascadeSet out{};
    const float nearZ = std::max(camera.nearZ, 0.01f);
    const float farZ = std::max(shadowDistance, nearZ + 1.0f);
    splitLambda = glm::clamp(splitLambda, 0.0f, 1.0f);
    for (int i = 1; i <= 3; ++i) {
        const float t = static_cast<float>(i) / 3.0f;
        const float logarithmic = nearZ * std::pow(farZ / nearZ, t);
        const float uniform = nearZ + (farZ - nearZ) * t;
        out.splitDepths[i - 1] = splitLambda * logarithmic + (1.0f - splitLambda) * uniform;
    }
    glm::vec3 dir = glm::dot(sunDirection, sunDirection) > 1.0e-12f
        ? glm::normalize(sunDirection) : glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));
    const glm::vec3 up = std::abs(dir.y) > 0.99f ? glm::vec3(1,0,0) : glm::vec3(0,1,0);
    const glm::vec3 right = camera.right();
    const glm::vec3 camUp = glm::normalize(glm::cross(right, camera.forward()));
    const float tanHalf = std::tan(glm::radians(camera.fovY) * 0.5f);
    float sliceNear = nearZ;
    for (int cascade = 0; cascade < 3; ++cascade) {
        const float sliceFar = out.splitDepths[cascade];
        glm::vec3 corners[8]; int k = 0;
        for (float depth : {sliceNear, sliceFar}) {
            const float halfH = tanHalf * depth;
            const float halfW = halfH * aspect;
            const glm::vec3 center = camera.position + camera.forward() * depth;
            corners[k++] = center - right*halfW - camUp*halfH;
            corners[k++] = center + right*halfW - camUp*halfH;
            corners[k++] = center - right*halfW + camUp*halfH;
            corners[k++] = center + right*halfW + camUp*halfH;
        }
        glm::vec3 center(0.0f);
        for (const auto& c : corners) center += c;
        center /= 8.0f;
        float radius = 0.0f;
        for (const auto& c : corners) radius = std::max(radius, glm::length(c-center));
        radius = std::max(radius, 1.0f);
        radius = std::ceil(radius * 16.0f) / 16.0f; // quantized radius reduces cascade-scale shimmer
        // Quantize the light-space center to shadow texels to prevent shimmer.
        const glm::mat4 orientation = glm::lookAt(glm::vec3(0.0f), dir, up);
        const float texel = (2.0f * radius) / static_cast<float>(std::max(mapSize,1));
        glm::vec3 lightCenter = glm::vec3(orientation * glm::vec4(center,1.0f));
        lightCenter.x = std::floor(lightCenter.x / texel) * texel;
        lightCenter.y = std::floor(lightCenter.y / texel) * texel;
        center = glm::vec3(glm::inverse(orientation) * glm::vec4(lightCenter,1.0f));
        const glm::mat4 view = glm::lookAt(center - dir * radius * 2.0f, center, up);
        const glm::mat4 projection = glm::ortho(-radius,radius,-radius,radius,0.1f,radius*4.5f);
        out.lightVP[cascade] = projection * view;
        out.texelWorld[cascade] = texel;
        sliceNear = sliceFar;
    }
    return out;
}

} // namespace anvil
