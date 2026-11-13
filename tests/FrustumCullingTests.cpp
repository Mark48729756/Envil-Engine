#include "anvil/Frustum.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    using namespace anvil;
    const Frustum gl = Frustum::fromViewProjection(glm::mat4(1.0f));
    assert(gl.intersectsSphere({0, 0, 0}, 0.1f));
    assert(gl.intersectsSphere({1.05f, 0, 0}, 0.1f));
    assert(!gl.intersectsSphere({1.2f, 0, 0}, 0.1f));
    assert(!gl.intersectsSphere({0, 0, -1.2f}, 0.1f));

    // D3D/Vulkan depth convention: near plane is z >= 0, far plane is z <= w.
    const Frustum zeroToOne = Frustum::fromViewProjection(glm::mat4(1.0f), true);
    assert(zeroToOne.intersectsSphere({0, 0, 0.05f}, 0.1f));
    assert(!zeroToOne.intersectsSphere({0, 0, -0.3f}, 0.1f));
    assert(!zeroToOne.intersectsSphere({0, 0, 1.2f}, 0.1f));

    glm::mat4 model(1.0f);
    model[0][0] = 2.0f; model[1][1] = 3.0f; model[2][2] = 0.5f;
    model[1][0] = 0.75f; // shear: the bound must remain conservative
    glm::vec3 center; float radius = 0.0f;
    transformSphere(model, {0, 0, 0}, 1.0f, center, radius);
    assert(std::isfinite(radius) && radius >= 3.0f);

    // Invalid bounds must fail open (render) rather than incorrectly disappear.
    assert(gl.intersectsSphere({NAN, 0, 0}, 1.0f));
    assert(gl.intersectsSphere({0, 0, 0}, NAN));
    std::cout << "Frustum culling tests passed\n";
}
