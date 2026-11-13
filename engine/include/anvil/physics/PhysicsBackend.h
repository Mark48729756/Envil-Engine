#pragma once
// ANVIL physics backend abstraction (M32).
//
// IPhysicsBackend is what Scene talks to. Two implementations exist:
//   * Jolt   - production rigid bodies (box / sphere / capsule, full rotation, CCD,
//              triggers, contact events, raycasts). Needs -DANVIL_WITH_JOLT=ON.
//   * Simple - the dependency-free AABB prototype (no rotation, spheres/capsules are
//              approximated by their bounding box). Used as a fallback only.
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>
#include <memory>
#include <vector>

namespace anvil::physics {

enum class ShapeType : std::uint8_t { Box = 0, Sphere = 1, Capsule = 2 };
enum class MotionType : std::uint8_t { Static = 0, Kinematic = 1, Dynamic = 2 };
enum class BackendKind : std::uint8_t { Auto = 0, Simple = 1, Jolt = 2 };

struct RigidBodyDesc {
    ShapeType shape = ShapeType::Box;
    MotionType motion = MotionType::Dynamic;
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 halfExtents{0.5f};   // Box
    float radius = 0.5f;           // Sphere / Capsule
    float halfHeight = 0.5f;       // Capsule: half height of the cylindrical part (Y axis)
    glm::vec3 shapeOffset{0.0f};   // collision shape offset in body-local space
    glm::vec3 linearVelocity{0.0f};
    glm::vec3 angularVelocity{0.0f};
    float mass = 1.0f;
    float friction = 0.6f;
    float restitution = 0.05f;
    float linearDamping = 0.02f;
    float angularDamping = 0.05f;
    float gravityScale = 1.0f;
    bool isTrigger = false;        // detects overlaps, produces no collision response
    bool continuous = false;       // continuous collision detection (fast bodies)
    bool allowSleeping = true;
};

struct BodyState {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 linearVelocity{0.0f};
    glm::vec3 angularVelocity{0.0f};
    bool active = false;           // false = sleeping / static
};

struct RayHit {
    std::uint32_t body = 0;
    float distance = 0.0f;
    glm::vec3 point{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
};

struct ContactEvent {
    std::uint32_t a = 0, b = 0;    // body handles
    bool begin = true;             // false = contact / overlap ended
    bool trigger = false;          // at least one body is a trigger
};

class IPhysicsBackend {
public:
    virtual ~IPhysicsBackend() = default;
    virtual const char* name() const = 0;
    virtual bool supportsRotation() const = 0;
    virtual bool supportsEvents() const = 0;

    // Handles are non-zero; 0 means "failed". Descriptions are sanitized (finite, clamped).
    virtual std::uint32_t createBody(const RigidBodyDesc& desc) = 0;
    virtual bool destroyBody(std::uint32_t handle) = 0;
    virtual std::size_t bodyCount() const = 0;

    virtual void step(float frameDelta) = 0;                       // fixed-step accumulator inside
    virtual bool getState(std::uint32_t handle, BodyState& out) const = 0;
    virtual bool setTransform(std::uint32_t handle, const glm::vec3& p, const glm::quat& q) = 0;           // teleport
    virtual bool moveKinematic(std::uint32_t handle, const glm::vec3& p, const glm::quat& q, float dt) = 0; // swept move
    virtual bool setLinearVelocity(std::uint32_t handle, const glm::vec3& v) = 0;
    virtual bool setAngularVelocity(std::uint32_t handle, const glm::vec3& w) = 0;
    virtual bool addForce(std::uint32_t handle, const glm::vec3& force) = 0;       // N, applied next step
    virtual bool addImpulse(std::uint32_t handle, const glm::vec3& impulse) = 0;   // N*s, immediate

    virtual bool raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, RayHit& out) const = 0;
    virtual void setGravity(const glm::vec3& g) = 0;
    virtual void drainEvents(std::vector<ContactEvent>& out) = 0;   // appends and clears
};

bool joltAvailable();
// Auto = Jolt when compiled in, otherwise Simple. Jolt requested but not compiled -> Simple.
std::unique_ptr<IPhysicsBackend> createBackend(BackendKind kind = BackendKind::Auto,
                                               const glm::vec3& gravity = {0.0f, -9.81f, 0.0f});
std::unique_ptr<IPhysicsBackend> createSimpleBackend(const glm::vec3& gravity);
#ifdef ANVIL_WITH_JOLT
std::unique_ptr<IPhysicsBackend> createJoltBackend(const glm::vec3& gravity);
#endif

} // namespace anvil::physics
