#include "anvil/physics/PhysicsWorld.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    using namespace anvil::physics;
    PhysicsWorld world;
    BodyDesc floor;
    floor.position = {0.0f, -1.0f, 0.0f};
    floor.halfExtents = {5.0f, 0.5f, 5.0f};
    floor.isStatic = true;
    floor.useGravity = false;
    const auto floorId = world.createBody(floor);

    BodyDesc box;
    box.position = {0.0f, 2.0f, 0.0f};
    box.halfExtents = {0.5f, 0.5f, 0.5f};
    box.linearDamping = 0.05f;
    const auto boxId = world.createBody(box);
    for (int i = 0; i < 240; ++i) world.step(1.0f / 60.0f);

    const Body* settled = world.getBody(boxId);
    assert(settled && settled->desc.position.y > -0.15f);
    assert(!world.contacts().empty());
    assert(world.getBody(floorId)->desc.isStatic);

    const float before = glm::length(settled->desc.velocity);
    world.step(1.0f / 60.0f);
    const Body* after = world.getBody(boxId);
    assert(after && std::isfinite(after->desc.position.y));
    assert(before < 1.0f); // contact resolution has removed most downward velocity

    bool rejected = false;
    try { world.setFixedDelta(0.0f); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    assert(world.destroyBody(boxId));
    assert(!world.destroyBody(boxId));
    std::cout << "Physics world tests passed\n";
}
