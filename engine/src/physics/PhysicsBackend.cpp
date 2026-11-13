#include "anvil/physics/PhysicsBackend.h"
#include "anvil/physics/PhysicsWorld.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace anvil::physics {

bool joltAvailable() {
#ifdef ANVIL_WITH_JOLT
    return true;
#else
    return false;
#endif
}

std::unique_ptr<IPhysicsBackend> createBackend(BackendKind kind, const glm::vec3& gravity) {
#ifdef ANVIL_WITH_JOLT
    if (kind == BackendKind::Auto || kind == BackendKind::Jolt) return createJoltBackend(gravity);
#endif
    return createSimpleBackend(gravity);
}

namespace {
bool finite3(const glm::vec3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

// Dependency-free fallback built on the AABB PhysicsWorld. Rotation is not simulated.
class SimpleBackend final : public IPhysicsBackend {
public:
    explicit SimpleBackend(const glm::vec3& g) : world_(g) {}
    const char* name() const override { return "Simple AABB (fallback)"; }
    bool supportsRotation() const override { return false; }
    bool supportsEvents() const override { return false; }

    std::uint32_t createBody(const RigidBodyDesc& d) override {
        BodyDesc b;
        switch (d.shape) {
            case ShapeType::Box:     b.halfExtents = d.halfExtents; break;
            case ShapeType::Sphere:  b.halfExtents = glm::vec3(d.radius); break;
            case ShapeType::Capsule: b.halfExtents = glm::vec3(d.radius, d.halfHeight + d.radius, d.radius); break;
        }
        b.position = finite3(d.position) ? d.position + d.shapeOffset : glm::vec3(0.0f);
        b.velocity = d.linearVelocity;
        b.mass = d.mass; b.restitution = d.restitution; b.friction = d.friction;
        b.linearDamping = d.linearDamping; b.gravityScale = d.gravityScale;
        b.isStatic = d.motion != MotionType::Dynamic;
        b.useGravity = d.motion == MotionType::Dynamic;
        const std::uint32_t id = world_.createBody(b);
        info_[id] = {d.shapeOffset, d.rotation, d.mass > 0.0f ? d.mass : 1.0f};
        return id;
    }
    bool destroyBody(std::uint32_t h) override { info_.erase(h); forces_.erase(h); return world_.destroyBody(h); }
    std::size_t bodyCount() const override { return info_.size(); }

    void step(float dt) override {
        if (std::isfinite(dt) && dt > 0.0f)
            for (auto& [h, f] : forces_)
                if (Body* b = world_.getBody(h)) b->desc.velocity += f / info_[h].mass * dt;
        forces_.clear();
        world_.step(dt);
    }
    bool getState(std::uint32_t h, BodyState& out) const override {
        const Body* b = world_.getBody(h); auto it = info_.find(h);
        if (!b || it == info_.end()) return false;
        out.position = b->desc.position - it->second.offset;
        out.rotation = it->second.rotation;
        out.linearVelocity = b->desc.velocity; out.angularVelocity = glm::vec3(0.0f);
        out.active = !b->desc.isStatic;
        return true;
    }
    bool setTransform(std::uint32_t h, const glm::vec3& p, const glm::quat& q) override {
        Body* b = world_.getBody(h); auto it = info_.find(h);
        if (!b || it == info_.end() || !finite3(p)) return false;
        b->desc.position = p + it->second.offset; it->second.rotation = q; return true;
    }
    bool moveKinematic(std::uint32_t h, const glm::vec3& p, const glm::quat& q, float) override { return setTransform(h, p, q); }
    bool setLinearVelocity(std::uint32_t h, const glm::vec3& v) override { Body* b = world_.getBody(h); if (!b || !finite3(v)) return false; b->desc.velocity = v; return true; }
    bool setAngularVelocity(std::uint32_t h, const glm::vec3&) override { return world_.getBody(h) != nullptr; }
    bool addForce(std::uint32_t h, const glm::vec3& f) override { if (!world_.getBody(h) || !finite3(f)) return false; forces_[h] += f; return true; }
    bool addImpulse(std::uint32_t h, const glm::vec3& j) override {
        Body* b = world_.getBody(h); auto it = info_.find(h);
        if (!b || it == info_.end() || b->desc.isStatic || !finite3(j)) return false;
        b->desc.velocity += j / it->second.mass; return true;
    }
    bool raycast(const glm::vec3& o, const glm::vec3& dir, float maxDist, RayHit& out) const override {
        const float len = glm::length(dir);
        if (!(len > 1e-8f) || !(maxDist > 0.0f)) return false;
        const glm::vec3 d = dir / len;
        float best = maxDist; bool hit = false;
        for (const auto& [h, inf] : info_) {
            (void)inf;
            const Body* b = world_.getBody(h); if (!b) continue;
            const glm::vec3 lo = b->desc.position - b->desc.halfExtents, hi = b->desc.position + b->desc.halfExtents;
            float t0 = 0.0f, t1 = best; int axisHit = -1; float sign = 1.0f; bool ok = true;
            for (int a = 0; a < 3 && ok; ++a) {
                if (std::abs(d[a]) < 1e-8f) { if (o[a] < lo[a] || o[a] > hi[a]) ok = false; continue; }
                float ta = (lo[a] - o[a]) / d[a], tb = (hi[a] - o[a]) / d[a]; float s = -1.0f;
                if (ta > tb) { std::swap(ta, tb); s = 1.0f; }
                if (ta > t0) { t0 = ta; axisHit = a; sign = s; }
                t1 = std::min(t1, tb);
                if (t0 > t1) ok = false;
            }
            if (!ok || axisHit < 0 || t0 >= best) continue;
            best = t0; hit = true; out.body = h; out.distance = t0; out.point = o + d * t0;
            out.normal = glm::vec3(0.0f); out.normal[axisHit] = sign;
        }
        return hit;
    }
    void setGravity(const glm::vec3& g) override { if (finite3(g)) world_.setGravity(g); }
    void drainEvents(std::vector<ContactEvent>&) override {}

private:
    struct Info { glm::vec3 offset; glm::quat rotation; float mass; };
    PhysicsWorld world_;
    std::unordered_map<std::uint32_t, Info> info_;
    std::unordered_map<std::uint32_t, glm::vec3> forces_;
};
} // namespace

std::unique_ptr<IPhysicsBackend> createSimpleBackend(const glm::vec3& gravity) { return std::make_unique<SimpleBackend>(gravity); }

} // namespace anvil::physics
