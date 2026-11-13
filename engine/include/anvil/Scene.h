#pragma once
#include "Renderer.h"
#include "ECS.h"
#include "physics/PhysicsBackend.h"
#include "physics/PhysicsWorld.h"
#include <filesystem>
#include <cstdint>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace anvil {

enum class Primitive { None, Sphere, Cube, Plane, Model };

// Authoring data of the Rigidbody component. It lives on the Entity so it is serialized,
// duplicated and restored by the editor's Play/Stop snapshot like every other component.
struct PhysicsComponent {
    bool enabled = false;
    physics::ShapeType shape = physics::ShapeType::Box;
    physics::MotionType motion = physics::MotionType::Dynamic;
    bool autoFit = true;                     // derive the collider from the mesh primitive and entity scale
    glm::vec3 halfExtents{1.0f};             // used when !autoFit (local space, multiplied by world scale)
    float radius = 1.0f, halfHeight = 0.5f;  // sphere / capsule (local space)
    float mass = 1.0f, friction = 0.6f, restitution = 0.05f;
    float linearDamping = 0.02f, angularDamping = 0.05f, gravityScale = 1.0f;
    bool isTrigger = false, continuous = false;
};

// Runtime ECS component (lives in Scene's ecs::Registry only while physics runs).
struct PhysicsBody { std::uint32_t handle = 0; physics::MotionType motion = physics::MotionType::Dynamic; };

struct SceneRayHit { uint32_t entity = 0; float distance = 0.0f; glm::vec3 point{0.0f}; glm::vec3 normal{0.0f, 1.0f, 0.0f}; };
struct SceneCollision { uint32_t a = 0, b = 0; bool begin = true; bool trigger = false; };   // entity ids

// Flat entity for now (hierarchy / parenting and a real ECS come in a later milestone).
struct Entity {
    uint32_t id = 0;
    std::string name = "Entity";
    bool active = true;
    uint32_t parentId = 0; // 0 means scene root; transforms are local to the parent

    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};   // euler degrees, applied Y * X * Z
    glm::vec3 scale{1.0f};

    Primitive primitive = Primitive::None;   // MeshRenderer
    Material material;
    std::string modelPath;
    std::shared_ptr<Mesh> modelMesh;

    bool isLight = false;                    // Point Light
    glm::vec3 lightColor{1.0f};
    float lightIntensity = 60.0f;

    bool orbit = false;                      // demo "script": orbits around orbitCenter while Play mode runs
    glm::vec3 orbitCenter{0.0f, 4.0f, 3.0f};
    float orbitRadius = 8.0f, orbitSpeed = 0.6f, orbitPhase = 0.0f;

    PhysicsComponent physics;                // Rigidbody (+ collider)

    glm::mat4 model() const;
};

class Scene {
public:
    void initGpuResources(const std::filesystem::path& projectRoot = std::filesystem::current_path());   // needs a current GL context

    Entity& add(const std::string& name);
    Entity* find(uint32_t id);
    void remove(uint32_t id);
    bool setParent(uint32_t childId, uint32_t parentId);
    glm::mat4 worldModel(uint32_t id) const;
    std::vector<uint32_t> childrenOf(uint32_t parentId) const;
    Entity& duplicate(uint32_t id);

    // local-space half extents used for picking
    static glm::vec3 localHalfExtents(const Entity& e);

    const Mesh* cubeMesh() const { return cube_.get(); }   // unit cube (half extent 1), valid after initGpuResources
    void collect(std::vector<DrawItem>& items, std::vector<PointLight>& lights) const;
    void updateOrbits(float time);

    // ---- Physics (M32): Entity::physics authoring data -> ECS runtime components -> IPhysicsBackend ----
    // startPhysics builds one backend body per enabled entity (world transform incl. parents and scale),
    // stepPhysics simulates and writes dynamic poses back to local transforms (parents supported),
    // pushes kinematic poses to the backend. stopPhysics destroys every body; authoring data is untouched.
    bool startPhysics(physics::BackendKind kind = physics::BackendKind::Auto);
    void stopPhysics();
    bool physicsRunning() const { return backend_ != nullptr; }
    void stepPhysics(float frameDelta);
    const char* physicsBackendName() const { return backend_ ? backend_->name() : "none"; }
    size_t physicsBodyCount() const { return backend_ ? backend_->bodyCount() : 0; }

    bool addImpulse(uint32_t entityId, const glm::vec3& impulse);
    bool addForce(uint32_t entityId, const glm::vec3& force);
    bool setLinearVelocity(uint32_t entityId, const glm::vec3& v);
    bool getLinearVelocity(uint32_t entityId, glm::vec3& out) const;
    bool teleportBody(uint32_t entityId);                       // re-sync backend body to the entity's current world transform
    bool raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, SceneRayHit& out) const;
    void drainCollisions(std::vector<SceneCollision>& out);    // begin/end contact + trigger events as entity ids
    void setGravity(const glm::vec3& g) { gravity_ = g; if (backend_) backend_->setGravity(g); }

    // Legacy M26 API, kept source-compatible: enables the Rigidbody component from a BodyDesc and starts physics if needed.
    bool attachPhysicsBody(uint32_t entityId, const physics::BodyDesc& desc);
    bool detachPhysicsBody(uint32_t entityId);

    ecs::Registry& registry() { return registry_; }
    const ecs::Registry& registry() const { return registry_; }

    // Used by validated scene deserialization to avoid reusing persistent IDs.
    void setNextId(uint32_t value) { nextId_ = value ? value : 1; }
    uint32_t nextId() const { return nextId_; }

    std::vector<Entity> entities;
    Sun sun;

private:
    uint32_t nextId_ = 1;
    std::unique_ptr<Mesh> sphere_, cube_, plane_;
    bool createRuntimeBody(const Entity& e);
    void destroyRuntimeBody(uint32_t entityId);
    ecs::Registry registry_;                                  // runtime components, entity id == ecs id
    std::unique_ptr<physics::IPhysicsBackend> backend_;
    std::unordered_map<uint32_t, uint32_t> handleToEntity_;   // backend handle -> scene entity id
    glm::vec3 gravity_{0.0f, -9.81f, 0.0f};
};

} // namespace anvil
