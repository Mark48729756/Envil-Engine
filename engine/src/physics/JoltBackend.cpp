#include "anvil/physics/PhysicsBackend.h"
#include "JoltCommon.h"
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace anvil::physics {
namespace {
using namespace jolt;

bool finite3(const glm::vec3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
float clampFinite(float v, float lo, float hi, float fallback) { return std::isfinite(v) ? std::clamp(v, lo, hi) : fallback; }
JPH::Vec3 J(const glm::vec3& v) { return {v.x, v.y, v.z}; }
JPH::RVec3 JR(const glm::vec3& v) { return {v.x, v.y, v.z}; }
JPH::Quat JQ(const glm::quat& q) { return JPH::Quat(q.x, q.y, q.z, q.w).Normalized(); }
glm::vec3 G(JPH::Vec3Arg v) { return {v.GetX(), v.GetY(), v.GetZ()}; }
glm::vec3 GR(JPH::RVec3Arg v) { return {float(v.GetX()), float(v.GetY()), float(v.GetZ())}; }
glm::quat GQ(JPH::QuatArg q) { return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ()); }

class JoltBackend final : private RegisterFirst, public IPhysicsBackend, private JPH::ContactListener {
public:
    explicit JoltBackend(const glm::vec3& gravity) {
        system_.Init(65536, 0, 65536, 10240, broadPhase_, objectVsBroadPhase_, objectPairs_);
        system_.SetGravity(J(finite3(gravity) ? gravity : glm::vec3(0.0f, -9.81f, 0.0f)));
        system_.SetContactListener(this);
    }
    ~JoltBackend() override {
        auto& bi = system_.GetBodyInterface();
        for (auto& [h, id] : bodies_) { bi.RemoveBody(id); bi.DestroyBody(id); }
    }
    const char* name() const override { return "Jolt Physics 5.2"; }
    bool supportsRotation() const override { return true; }
    bool supportsEvents() const override { return true; }

    std::uint32_t createBody(const RigidBodyDesc& d) override {
        JPH::Ref<JPH::Shape> shape = makeShape(d);
        if (!shape) return 0;
        const bool isStatic = d.motion == MotionType::Static;
        const auto motion = isStatic ? JPH::EMotionType::Static : d.motion == MotionType::Kinematic ? JPH::EMotionType::Kinematic : JPH::EMotionType::Dynamic;
        glm::vec3 pos = finite3(d.position) ? d.position : glm::vec3(0.0f);
        JPH::BodyCreationSettings s(shape, JR(pos), JQ(d.rotation), motion, isStatic ? kStaticLayer : kMovingLayer);
        s.mFriction = clampFinite(d.friction, 0.0f, 1.0f, 0.6f);
        s.mRestitution = clampFinite(d.restitution, 0.0f, 1.0f, 0.05f);
        s.mLinearDamping = clampFinite(d.linearDamping, 0.0f, 20.0f, 0.02f);
        s.mAngularDamping = clampFinite(d.angularDamping, 0.0f, 20.0f, 0.05f);
        s.mGravityFactor = clampFinite(d.gravityScale, -10.0f, 10.0f, 1.0f);
        s.mIsSensor = d.isTrigger;
        s.mAllowSleeping = d.allowSleeping;
        if (motion == JPH::EMotionType::Dynamic) {
            s.mMotionQuality = d.continuous ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;
            s.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;   // keep real inertia from the shape
            s.mMassPropertiesOverride.mMass = clampFinite(d.mass, 0.0001f, 1.0e7f, 1.0f);
            if (finite3(d.linearVelocity)) s.mLinearVelocity = J(d.linearVelocity);
            if (finite3(d.angularVelocity)) s.mAngularVelocity = J(d.angularVelocity);
        }
        auto& bi = system_.GetBodyInterface();
        JPH::Body* body = bi.CreateBody(s);
        if (!body) return 0;
        const std::uint32_t handle = nextHandle_++;
        body->SetUserData(handle);
        const JPH::BodyID id = body->GetID();
        { std::lock_guard<std::mutex> lock(mapMutex_); bodies_.emplace(handle, id); byId_.emplace(id.GetIndexAndSequenceNumber(), handle); }
        bi.AddBody(id, isStatic ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);
        return handle;
    }
    bool destroyBody(std::uint32_t h) override {
        JPH::BodyID id;
        { std::lock_guard<std::mutex> lock(mapMutex_); auto it = bodies_.find(h); if (it == bodies_.end()) return false; id = it->second; byId_.erase(id.GetIndexAndSequenceNumber()); bodies_.erase(it); }
        auto& bi = system_.GetBodyInterface(); bi.RemoveBody(id); bi.DestroyBody(id); return true;
    }
    std::size_t bodyCount() const override { std::lock_guard<std::mutex> lock(mapMutex_); return bodies_.size(); }

    void step(float frameDelta) override {
        if (!std::isfinite(frameDelta) || frameDelta <= 0.0f) return;
        accumulator_ += std::min(frameDelta, 0.25f);
        unsigned n = 0;
        while (accumulator_ >= kFixed && n < 8) { system_.Update(kFixed, 1, &allocator_, &jobs_); accumulator_ -= kFixed; ++n; }
        if (n == 8 && accumulator_ >= kFixed) accumulator_ = std::fmod(accumulator_, kFixed);
    }
    bool getState(std::uint32_t h, BodyState& out) const override {
        JPH::BodyID id; if (!lookup(h, id)) return false;
        const auto& bi = system_.GetBodyInterfaceNoLock();
        JPH::RVec3 p; JPH::Quat q; bi.GetPositionAndRotation(id, p, q);
        out.position = GR(p); out.rotation = GQ(q);
        out.linearVelocity = G(bi.GetLinearVelocity(id)); out.angularVelocity = G(bi.GetAngularVelocity(id));
        out.active = bi.IsActive(id); return true;
    }
    bool setTransform(std::uint32_t h, const glm::vec3& p, const glm::quat& q) override {
        JPH::BodyID id; if (!lookup(h, id) || !finite3(p)) return false;
        system_.GetBodyInterface().SetPositionAndRotation(id, JR(p), JQ(q), JPH::EActivation::Activate); return true;
    }
    bool moveKinematic(std::uint32_t h, const glm::vec3& p, const glm::quat& q, float dt) override {
        JPH::BodyID id; if (!lookup(h, id) || !finite3(p) || !(dt > 0.0f)) return false;
        system_.GetBodyInterface().MoveKinematic(id, JR(p), JQ(q), dt); return true;
    }
    bool setLinearVelocity(std::uint32_t h, const glm::vec3& v) override { JPH::BodyID id; if (!lookup(h, id) || !finite3(v)) return false; system_.GetBodyInterface().SetLinearVelocity(id, J(v)); return true; }
    bool setAngularVelocity(std::uint32_t h, const glm::vec3& w) override { JPH::BodyID id; if (!lookup(h, id) || !finite3(w)) return false; system_.GetBodyInterface().SetAngularVelocity(id, J(w)); return true; }
    bool addForce(std::uint32_t h, const glm::vec3& f) override { JPH::BodyID id; if (!lookup(h, id) || !finite3(f)) return false; system_.GetBodyInterface().AddForce(id, J(f)); return true; }
    bool addImpulse(std::uint32_t h, const glm::vec3& j) override { JPH::BodyID id; if (!lookup(h, id) || !finite3(j)) return false; system_.GetBodyInterface().AddImpulse(id, J(j)); return true; }

    bool raycast(const glm::vec3& o, const glm::vec3& dir, float maxDist, RayHit& out) const override {
        const float len = glm::length(dir);
        if (!finite3(o) || !finite3(dir) || !(len > 1e-8f) || !(maxDist > 0.0f)) return false;
        const glm::vec3 d = dir / len;
        JPH::RRayCast ray(JR(o), J(d * maxDist));
        JPH::RayCastResult hit;
        if (!system_.GetNarrowPhaseQuery().CastRay(ray, hit)) return false;
        const JPH::RVec3 point = ray.GetPointOnRay(hit.mFraction);
        out.distance = hit.mFraction * maxDist; out.point = GR(point);
        JPH::BodyLockRead lock(system_.GetBodyLockInterface(), hit.mBodyID);
        out.body = 0; out.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        if (lock.Succeeded()) {
            const JPH::Body& b = lock.GetBody();
            out.body = std::uint32_t(b.GetUserData());
            out.normal = G(b.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, point));
        }
        return out.body != 0;
    }
    void setGravity(const glm::vec3& g) override { if (finite3(g)) system_.SetGravity(J(g)); }
    void drainEvents(std::vector<ContactEvent>& out) override {
        std::lock_guard<std::mutex> lock(eventMutex_);
        out.insert(out.end(), events_.begin(), events_.end()); events_.clear();
    }

private:
    static constexpr float kFixed = 1.0f / 60.0f;

    bool lookup(std::uint32_t h, JPH::BodyID& id) const {
        std::lock_guard<std::mutex> lock(mapMutex_);
        auto it = bodies_.find(h); if (it == bodies_.end()) return false; id = it->second; return true;
    }
    static JPH::Ref<JPH::Shape> makeShape(const RigidBodyDesc& d) {
        JPH::ShapeSettings::ShapeResult r;
        switch (d.shape) {
            case ShapeType::Box: {
                const glm::vec3 he = glm::max(glm::abs(d.halfExtents), glm::vec3(0.005f));
                const float cr = std::min(0.05f, std::min({he.x, he.y, he.z}) * 0.5f);   // Jolt requires convex radius <= half extent
                r = JPH::BoxShapeSettings(J(he), cr).Create(); break; }
            case ShapeType::Sphere:
                r = JPH::SphereShapeSettings(std::max(std::abs(d.radius), 0.005f)).Create(); break;
            case ShapeType::Capsule:
                r = JPH::CapsuleShapeSettings(std::max(std::abs(d.halfHeight), 0.005f), std::max(std::abs(d.radius), 0.005f)).Create(); break;
        }
        if (r.HasError()) return nullptr;
        JPH::Ref<JPH::Shape> shape = r.Get();
        if (finite3(d.shapeOffset) && glm::length(d.shapeOffset) > 1e-6f) {
            auto wrapped = JPH::RotatedTranslatedShapeSettings(J(d.shapeOffset), JPH::Quat::sIdentity(), shape).Create();
            if (wrapped.HasError()) return nullptr;
            shape = wrapped.Get();
        }
        return shape;
    }
    static std::uint32_t userHandle(const JPH::Body& b) { return std::uint32_t(b.GetUserData()); }
    void push(std::uint32_t a, std::uint32_t b, bool begin, bool trigger) {
        std::lock_guard<std::mutex> lock(eventMutex_);
        if (events_.size() < 65536) events_.push_back({a, b, begin, trigger});
    }
    // JPH::ContactListener: called from worker threads during Update().
    void OnContactAdded(const JPH::Body& a, const JPH::Body& b, const JPH::ContactManifold&, JPH::ContactSettings&) override {
        push(userHandle(a), userHandle(b), true, a.IsSensor() || b.IsSensor());
    }
    void OnContactRemoved(const JPH::SubShapeIDPair& pair) override {
        std::uint32_t a = 0, b = 0;
        { std::lock_guard<std::mutex> lock(mapMutex_);
          auto ia = byId_.find(pair.GetBody1ID().GetIndexAndSequenceNumber()); auto ib = byId_.find(pair.GetBody2ID().GetIndexAndSequenceNumber());
          if (ia == byId_.end() || ib == byId_.end()) return;   // body already destroyed
          a = ia->second; b = ib->second; }
        push(a, b, false, false);
    }

    BroadPhaseLayers broadPhase_;
    ObjectVsBroadPhase objectVsBroadPhase_;
    ObjectPairs objectPairs_;
    JPH::PhysicsSystem system_;
    JPH::TempAllocatorImpl allocator_{32 * 1024 * 1024};
    JPH::JobSystemThreadPool jobs_{JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::max(1, int(std::thread::hardware_concurrency()) - 1)};
    mutable std::mutex mapMutex_, eventMutex_;
    std::unordered_map<std::uint32_t, JPH::BodyID> bodies_;
    std::unordered_map<std::uint32_t, std::uint32_t> byId_;
    std::vector<ContactEvent> events_;
    std::uint32_t nextHandle_ = 1;
    float accumulator_ = 0.0f;
};
} // namespace

std::unique_ptr<IPhysicsBackend> createJoltBackend(const glm::vec3& gravity) { return std::make_unique<JoltBackend>(gravity); }

} // namespace anvil::physics
