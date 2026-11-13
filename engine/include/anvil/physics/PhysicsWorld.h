#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

namespace anvil::physics {
struct BodyDesc {
    glm::vec3 position{0.0f};
    glm::vec3 halfExtents{0.5f};
    glm::vec3 velocity{0.0f};
    float mass = 1.0f;
    float restitution = 0.05f;
    float friction = 0.65f;
    float linearDamping = 0.02f; // exponential velocity damping per second
    float gravityScale = 1.0f;
    bool isStatic = false;
    bool useGravity = true;
};
struct Body { std::uint32_t id=0; BodyDesc desc; };
struct Contact { std::uint32_t a=0,b=0; glm::vec3 normal{0.0f}; float penetration=0.0f; };

// Deterministic fixed-step AABB rigid-body prototype. Rotation and continuous collision
// detection are intentionally outside this first CPU solver increment.
class PhysicsWorld {
public:
    explicit PhysicsWorld(glm::vec3 gravity={0.0f,-9.81f,0.0f});
    std::uint32_t createBody(const BodyDesc& desc);
    bool destroyBody(std::uint32_t id);
    Body* getBody(std::uint32_t id);
    const Body* getBody(std::uint32_t id) const;
    void step(float frameDelta);
    const std::vector<Contact>& contacts() const { return contacts_; }
    glm::vec3 gravity() const { return gravity_; }
    void setGravity(glm::vec3 value);
    float fixedDelta() const { return fixedDelta_; }
    void setFixedDelta(float value);
    unsigned maxSubsteps = 8;
private:
    void simulate(float dt);
    glm::vec3 gravity_;
    float fixedDelta_=1.0f/60.0f, accumulator_=0.0f;
    std::uint32_t nextId_=1;
    std::vector<Body> bodies_;
    std::vector<Contact> contacts_;
};
}
