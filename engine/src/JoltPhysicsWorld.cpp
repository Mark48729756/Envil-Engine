#include "anvil/physics/JoltPhysicsWorld.h"
#include "physics/JoltCommon.h"
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <thread>
#include <mutex>

namespace anvil::physics {
namespace {
constexpr JPH::uint kMaxBodies = 65536;
constexpr JPH::uint kNumBodyMutexes = 0;
constexpr JPH::uint kMaxBodyPairs = 65536;
constexpr JPH::uint kMaxContactConstraints = 10240;
constexpr JPH::ObjectLayer kStaticLayer = 0;
constexpr JPH::ObjectLayer kMovingLayer = 1;
constexpr JPH::BroadPhaseLayer kStaticBroadPhase(0);
constexpr JPH::BroadPhaseLayer kMovingBroadPhase(1);

class BroadPhaseLayers final : public JPH::BroadPhaseLayerInterface {
public:
    JPH::uint GetNumBroadPhaseLayers() const override { return 2; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        return layer == kStaticLayer ? kStaticBroadPhase : kMovingBroadPhase;
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
        return layer == kStaticBroadPhase ? "Static" : "Moving";
    }
#endif
};
class ObjectVsBroadPhase final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer object, JPH::BroadPhaseLayer broad) const override {
        return object == kMovingLayer || broad == kMovingBroadPhase;
    }
};
class ObjectPairs final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
        return a == kMovingLayer || b == kMovingLayer;
    }
};
}

struct JoltPhysicsWorld::Impl : jolt::RegisterFirst {   // M32: registration must precede member construction
    BroadPhaseLayers broadPhase;
    ObjectVsBroadPhase objectVsBroadPhase;
    ObjectPairs objectPairs;
    JPH::PhysicsSystem system;
    JPH::TempAllocatorImpl allocator{16 * 1024 * 1024};
    JPH::JobSystemThreadPool jobs{JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
        std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 1)};
    std::unordered_map<std::uint32_t, JPH::BodyID> bodies;
    std::uint32_t nextHandle = 1;
    float fixedDelta = 1.0f / 60.0f;
    float accumulator = 0.0f;

    Impl(glm::vec3 gravity) {
        system.Init(kMaxBodies, kNumBodyMutexes, kMaxBodyPairs,
                    kMaxContactConstraints, broadPhase, objectVsBroadPhase, objectPairs);
        system.SetGravity(JPH::Vec3(gravity.x, gravity.y, gravity.z));
    }
};

JoltPhysicsWorld::JoltPhysicsWorld(glm::vec3 gravity) : impl_(std::make_unique<Impl>(gravity)) {}
JoltPhysicsWorld::~JoltPhysicsWorld() {
    if (!impl_) return;
    auto& bi = impl_->system.GetBodyInterface();
    for (const auto& entry : impl_->bodies) { bi.RemoveBody(entry.second); bi.DestroyBody(entry.second); }
}
std::uint32_t JoltPhysicsWorld::createBox(glm::vec3 position, glm::vec3 halfExtents,
                                          float mass, bool isStatic, float friction, float restitution) {
    halfExtents = glm::max(glm::abs(halfExtents), glm::vec3(0.001f));
    auto shape = new JPH::BoxShape(JPH::Vec3(halfExtents.x, halfExtents.y, halfExtents.z));
    const auto motion = isStatic ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic;
    const auto layer = isStatic ? kStaticLayer : kMovingLayer;
    JPH::BodyCreationSettings settings(shape, JPH::RVec3(position.x, position.y, position.z),
                                       JPH::Quat::sIdentity(), motion, layer);
    settings.mFriction = std::clamp(friction, 0.0f, 1.0f);
    settings.mRestitution = std::clamp(restitution, 0.0f, 1.0f);
    // M32 fix: "MassAndInertiaProvided" with only a mass set gave bodies a zero inertia tensor.
    if (!isStatic) {
        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        if (mass > 0.0f) settings.mMassPropertiesOverride.mMass = mass;
    }
    auto& bi = impl_->system.GetBodyInterface();
    JPH::Body* body = bi.CreateBody(settings);
    if (!body) throw std::runtime_error("Jolt failed to create rigid body");
    const JPH::BodyID id = body->GetID();
    bi.AddBody(id, isStatic ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);
    const std::uint32_t handle = impl_->nextHandle++;
    impl_->bodies.emplace(handle, id);
    return handle;
}
bool JoltPhysicsWorld::destroyBody(std::uint32_t handle) {
    auto it = impl_->bodies.find(handle);
    if (it == impl_->bodies.end()) return false;
    auto& bi = impl_->system.GetBodyInterface();
    bi.RemoveBody(it->second); bi.DestroyBody(it->second);
    impl_->bodies.erase(it);
    return true;
}
void JoltPhysicsWorld::step(float frameDelta) {
    if (!std::isfinite(frameDelta) || frameDelta <= 0.0f) return;
    impl_->accumulator += std::min(frameDelta, 0.25f);
    unsigned steps = 0;
    while (impl_->accumulator >= impl_->fixedDelta && steps < 8) {
        impl_->system.Update(impl_->fixedDelta, 1, &impl_->allocator, &impl_->jobs);
        impl_->accumulator -= impl_->fixedDelta; ++steps;
    }
    if (steps == 8 && impl_->accumulator >= impl_->fixedDelta)
        impl_->accumulator = std::fmod(impl_->accumulator, impl_->fixedDelta);
}
bool JoltPhysicsWorld::getPosition(std::uint32_t handle, glm::vec3& out) const {
    auto it = impl_->bodies.find(handle);
    if (it == impl_->bodies.end()) return false;
    const JPH::RVec3 p = impl_->system.GetBodyInterface().GetPosition(it->second);
    out = {static_cast<float>(p.GetX()), static_cast<float>(p.GetY()), static_cast<float>(p.GetZ())};
    return true;
}
void JoltPhysicsWorld::setGravity(glm::vec3 g) {
    impl_->system.SetGravity(JPH::Vec3(g.x, g.y, g.z));
}
} // namespace anvil::physics
