#pragma once

#include <array>
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

namespace anvil {

// Conservative sphere-frustum test shared by the OpenGL and RHI renderers.
// Matrices are column-major GLM clip-from-world matrices.
struct Frustum {
    std::array<glm::vec4, 6> planes{}; // inward-facing, normalized (xyz normal, w distance)

    static Frustum fromViewProjection(const glm::mat4& m, bool zeroToOneDepth = false) {
        const glm::vec4 r0(m[0][0], m[1][0], m[2][0], m[3][0]);
        const glm::vec4 r1(m[0][1], m[1][1], m[2][1], m[3][1]);
        const glm::vec4 r2(m[0][2], m[1][2], m[2][2], m[3][2]);
        const glm::vec4 r3(m[0][3], m[1][3], m[2][3], m[3][3]);
        Frustum f;
        f.planes = {r3 + r0, r3 - r0, r3 + r1, r3 - r1,
                    zeroToOneDepth ? r2 : r3 + r2, r3 - r2};
        for (glm::vec4& p : f.planes) {
            const float n = glm::length(glm::vec3(p));
            if (n > 1e-7f) p /= n;
        }
        return f;
    }

    bool intersectsSphere(const glm::vec3& center, float radius) const {
        if (!std::isfinite(radius) || radius < 0.0f ||
            !std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z)) return true;
        for (const glm::vec4& p : planes)
            if (glm::dot(glm::vec3(p), center) + p.w < -radius) return false;
        return true;
    }
};

inline void transformSphere(const glm::mat4& model, const glm::vec3& localCenter, float localRadius,
                            glm::vec3& worldCenter, float& worldRadius) {
    const glm::vec4 c = model * glm::vec4(localCenter, 1.0f);
    worldCenter = glm::vec3(c) / (std::abs(c.w) > 1e-7f ? c.w : 1.0f);
    // sqrt(||A||_1 * ||A||_inf) bounds the largest singular value, including shear.
    float norm1 = 0.0f, normInf = 0.0f;
    for (int col = 0; col < 3; ++col) {
        float sum = 0.0f;
        for (int row = 0; row < 3; ++row) sum += std::abs(model[col][row]);
        norm1 = std::max(norm1, sum);
    }
    for (int row = 0; row < 3; ++row) {
        float sum = 0.0f;
        for (int col = 0; col < 3; ++col) sum += std::abs(model[col][row]);
        normInf = std::max(normInf, sum);
    }
    const float scaleBound = std::sqrt(norm1 * normInf);
    worldRadius = std::max(0.0f, localRadius) * scaleBound;
}

} // namespace anvil
