#pragma once

// Available when ANVIL_WITH_JOLT is enabled in CMake.
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>

namespace anvil::physics {

// Thin production-physics adapter. This coexists with the dependency-free prototype
// PhysicsWorld so projects can migrate deliberately and keep their existing API stable.
class JoltPhysicsWorld {
public:
    explicit JoltPhysicsWorld(glm::vec3 gravity = {0.0f, -9.81f, 0.0f});
    ~JoltPhysicsWorld();
    JoltPhysicsWorld(const JoltPhysicsWorld&) = delete;
    JoltPhysicsWorld& operator=(const JoltPhysicsWorld&) = delete;

    std::uint32_t createBox(glm::vec3 position, glm::vec3 halfExtents,
                            float mass = 1.0f, bool isStatic = false,
                            float friction = 0.6f, float restitution = 0.05f);
    bool destroyBody(std::uint32_t handle);
    void step(float frameDelta);
    bool getPosition(std::uint32_t handle, glm::vec3& outPosition) const;
    void setGravity(glm::vec3 gravity);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace anvil::physics
