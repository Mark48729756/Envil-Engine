#include "anvil/Scene.h"
#include "anvil/Texture2D.h"
#include "anvil/Core.h"
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <glm/gtx/matrix_decompose.hpp>

namespace anvil {

glm::mat4 Entity::model() const {
    glm::mat4 m = glm::translate(glm::mat4(1.0f), position);
    m *= glm::mat4_cast(glm::quat(glm::radians(glm::vec3(rotation.x, rotation.y, rotation.z))));
    return glm::scale(m, scale);
}

void Scene::initGpuResources(const std::filesystem::path& projectRoot) {
    sphere_ = Mesh::sphere(64);
    cube_ = Mesh::cube();
    plane_ = Mesh::plane(40.0f, 40.0f);
    for (Entity& e : entities) {
        if (!e.modelPath.empty()) { std::string loadError; std::filesystem::path model(e.modelPath); if(model.is_relative()) model=projectRoot/model; e.modelMesh = std::shared_ptr<Mesh>(Mesh::loadObj(model, &loadError).release()); if (!e.modelMesh) ANVIL_LOG("OBJ import failed (%s): %s", e.modelPath.c_str(), loadError.c_str()); }
        if (!e.material.albedoTexturePath.empty()) {
            std::filesystem::path p(e.material.albedoTexturePath);
            if (p.is_relative()) p = projectRoot / p;
            e.material.albedoTexture = Texture2D::load(p, true);
        }
    }
}

Entity& Scene::add(const std::string& name) {
    Entity e;
    e.id = nextId_++;
    e.name = name;
    entities.push_back(e);
    return entities.back();
}

Entity* Scene::find(uint32_t id) {
    for (Entity& e : entities) if (e.id == id) return &e;
    return nullptr;
}

void Scene::remove(uint32_t id) {
    destroyRuntimeBody(id);
    // Deleting a parent promotes its children to the root instead of deleting them.
    for (Entity& e : entities) if (e.parentId == id) e.parentId = 0;
    entities.erase(std::remove_if(entities.begin(), entities.end(), [&](const Entity& e) { return e.id == id; }), entities.end());
}

bool Scene::setParent(uint32_t childId, uint32_t parentId) {
    Entity* child = find(childId);
    if (!child || childId == parentId) return false;
    if (parentId && !find(parentId)) return false;
    // Reject cycles before changing the hierarchy.
    uint32_t cursor = parentId;
    size_t guard = 0;
    while (cursor && guard++ <= entities.size()) {
        if (cursor == childId) return false;
        const Entity* node = nullptr;
        for (const Entity& e : entities) if (e.id == cursor) { node = &e; break; }
        if (!node) break;
        cursor = node->parentId;
    }
    child->parentId = parentId;
    if (backend_) teleportBody(childId);   // world transform changed with the new parent; keep the body in sync
    return true;
}

glm::mat4 Scene::worldModel(uint32_t id) const {
    const Entity* node = nullptr;
    for (const Entity& e : entities) if (e.id == id) { node = &e; break; }
    if (!node) return glm::mat4(1.0f);
    glm::mat4 local = node->model();
    if (!node->parentId) return local;
    // A bounded walk prevents malformed/cyclic data from hanging the editor.
    glm::mat4 result = local;
    uint32_t parent = node->parentId;
    size_t guard = 0;
    while (parent && guard++ < entities.size()) {
        const Entity* p = nullptr;
        for (const Entity& e : entities) if (e.id == parent) { p = &e; break; }
        if (!p) break;
        result = p->model() * result;
        parent = p->parentId;
    }
    return result;
}

std::vector<uint32_t> Scene::childrenOf(uint32_t parentId) const {
    std::vector<uint32_t> result;
    for (const Entity& e : entities) if (e.parentId == parentId) result.push_back(e.id);
    return result;
}

Entity& Scene::duplicate(uint32_t id) {
    Entity* source = find(id);
    if (!source) throw std::invalid_argument("Cannot duplicate missing entity");
    Entity copy = *source;
    copy.id = nextId_++;
    copy.name += " (copy)";
    copy.position += glm::vec3(1.5f, 0.0f, 0.0f);
    entities.push_back(copy);
    return entities.back();
}

glm::vec3 Scene::localHalfExtents(const Entity& e) {
    switch (e.primitive) {
        case Primitive::Sphere: return glm::vec3(1.0f);
        case Primitive::Cube:   return glm::vec3(1.0f);
        case Primitive::Plane:  return glm::vec3(40.0f, 0.05f, 40.0f);
        default:                return glm::vec3(0.35f);   // light / empty: small pick box
    }
}

void Scene::collect(std::vector<DrawItem>& items, std::vector<PointLight>& lights) const {
    items.clear();
    lights.clear();
    for (const Entity& e : entities) {
        if (!e.active) continue;
        const Mesh* mesh = e.primitive == Primitive::Sphere ? sphere_.get()
                         : e.primitive == Primitive::Cube   ? cube_.get()
                          : e.primitive == Primitive::Plane  ? plane_.get()
                         : e.primitive == Primitive::Model  ? e.modelMesh.get() : nullptr;
        if (mesh) items.push_back({mesh, worldModel(e.id), e.material});
        if (e.isLight) lights.push_back({glm::vec3(worldModel(e.id)[3]), e.lightColor, e.lightIntensity});
    }
}


// ------------------------------------------------------------------ physics (M32)
namespace {
struct WorldPose { glm::vec3 position, scale; glm::quat rotation; };
WorldPose decomposeWorld(const glm::mat4& w) {
    WorldPose p;
    p.position = glm::vec3(w[3]);
    p.scale = {glm::length(glm::vec3(w[0])), glm::length(glm::vec3(w[1])), glm::length(glm::vec3(w[2]))};
    glm::mat3 r(glm::vec3(w[0]) / std::max(p.scale.x, 1e-6f), glm::vec3(w[1]) / std::max(p.scale.y, 1e-6f), glm::vec3(w[2]) / std::max(p.scale.z, 1e-6f));
    p.rotation = glm::normalize(glm::quat_cast(r));
    return p;
}
} // namespace

bool Scene::createRuntimeBody(const Entity& e) {
    if (!backend_ || !e.physics.enabled || !e.active) return false;
    const PhysicsComponent& c = e.physics;
    const WorldPose w = decomposeWorld(worldModel(e.id));
    const glm::vec3 sc = glm::max(glm::abs(w.scale), glm::vec3(0.001f));
    const float maxScale = std::max({sc.x, sc.y, sc.z});

    physics::RigidBodyDesc d;
    d.motion = c.motion; d.position = w.position; d.rotation = w.rotation;
    d.mass = c.mass; d.friction = c.friction; d.restitution = c.restitution;
    d.linearDamping = c.linearDamping; d.angularDamping = c.angularDamping; d.gravityScale = c.gravityScale;
    d.isTrigger = c.isTrigger; d.continuous = c.continuous;

    physics::ShapeType shape = c.shape;
    if (c.autoFit && (e.primitive == Primitive::Sphere || e.primitive == Primitive::Cube || e.primitive == Primitive::Plane)) {
        if (e.primitive == Primitive::Sphere) { shape = physics::ShapeType::Sphere; d.radius = 1.0f * maxScale; }
        else if (e.primitive == Primitive::Cube) { shape = physics::ShapeType::Box; d.halfExtents = sc; }
        else {   // the plane mesh has no thickness: give it a 1 unit slab below the surface so bodies cannot tunnel through
            shape = physics::ShapeType::Box; d.halfExtents = {40.0f * sc.x, 0.5f, 40.0f * sc.z}; d.shapeOffset = {0.0f, -0.5f, 0.0f};
        }
    } else if (shape == physics::ShapeType::Sphere) d.radius = c.radius * maxScale;
    else if (shape == physics::ShapeType::Capsule) { d.radius = c.radius * std::max(sc.x, sc.z); d.halfHeight = c.halfHeight * sc.y; }
    else d.halfExtents = c.halfExtents * sc;
    d.shape = shape;

    const std::uint32_t handle = backend_->createBody(d);
    if (!handle) { ANVIL_LOG("Physics: failed to create body for entity %u (%s)", e.id, e.name.c_str()); return false; }
    if (!registry_.valid(e.id)) registry_.createWithId(e.id);
    registry_.emplace<PhysicsBody>(e.id, PhysicsBody{handle, c.motion});
    handleToEntity_[handle] = e.id;
    return true;
}

void Scene::destroyRuntimeBody(uint32_t id) {
    if (!registry_.valid(id)) return;
    if (const PhysicsBody* b = registry_.get<PhysicsBody>(id)) {
        if (backend_) backend_->destroyBody(b->handle);
        handleToEntity_.erase(b->handle);
    }
    registry_.destroy(id);
}

bool Scene::startPhysics(physics::BackendKind kind) {
    stopPhysics();
    backend_ = physics::createBackend(kind, gravity_);
    if (!backend_) return false;
    for (const Entity& e : entities) createRuntimeBody(e);
    ANVIL_LOG("Physics started: %s, %zu bodies", backend_->name(), backend_->bodyCount());
    return true;
}

void Scene::stopPhysics() {
    backend_.reset();            // destroys every body
    handleToEntity_.clear();
    registry_.clear();
}

void Scene::stepPhysics(float frameDelta) {
    if (!backend_ || !std::isfinite(frameDelta) || frameDelta <= 0.0f) return;
    std::unordered_map<uint32_t, Entity*> index;
    index.reserve(entities.size());
    for (Entity& e : entities) index.emplace(e.id, &e);
    const auto bodies = registry_.view<PhysicsBody>();

    for (uint32_t id : bodies) {   // kinematic: entity transform drives the body
        const PhysicsBody* b = registry_.get<PhysicsBody>(id);
        auto it = index.find(id);
        if (!b || b->motion != physics::MotionType::Kinematic || it == index.end()) continue;
        const WorldPose w = decomposeWorld(worldModel(id));
        backend_->moveKinematic(b->handle, w.position, w.rotation, std::min(frameDelta, 0.25f));
    }
    backend_->step(frameDelta);

    for (uint32_t id : bodies) {   // dynamic: body drives the entity transform
        const PhysicsBody* b = registry_.get<PhysicsBody>(id);
        auto it = index.find(id);
        physics::BodyState st;
        if (!b || b->motion != physics::MotionType::Dynamic || it == index.end() || !backend_->getState(b->handle, st)) continue;
        Entity& e = *it->second;
        if (!e.parentId) { e.position = st.position; e.rotation = glm::degrees(glm::eulerAngles(st.rotation)); continue; }
        const glm::mat4 worldPose = glm::translate(glm::mat4(1.0f), st.position) * glm::mat4_cast(st.rotation);
        const glm::mat4 local = glm::inverse(worldModel(e.parentId)) * worldPose;
        const WorldPose lp = decomposeWorld(local);
        e.position = lp.position; e.rotation = glm::degrees(glm::eulerAngles(lp.rotation));
    }
}

bool Scene::addImpulse(uint32_t id, const glm::vec3& v) { const PhysicsBody* b = registry_.get<PhysicsBody>(id); return backend_ && b && backend_->addImpulse(b->handle, v); }
bool Scene::addForce(uint32_t id, const glm::vec3& v) { const PhysicsBody* b = registry_.get<PhysicsBody>(id); return backend_ && b && backend_->addForce(b->handle, v); }
bool Scene::setLinearVelocity(uint32_t id, const glm::vec3& v) { const PhysicsBody* b = registry_.get<PhysicsBody>(id); return backend_ && b && backend_->setLinearVelocity(b->handle, v); }
bool Scene::getLinearVelocity(uint32_t id, glm::vec3& out) const {
    const PhysicsBody* b = registry_.get<PhysicsBody>(id); physics::BodyState st;
    if (!backend_ || !b || !backend_->getState(b->handle, st)) return false;
    out = st.linearVelocity; return true;
}
bool Scene::teleportBody(uint32_t id) {
    const PhysicsBody* b = registry_.get<PhysicsBody>(id);
    if (!backend_ || !b) return false;
    const WorldPose w = decomposeWorld(worldModel(id));
    return backend_->setTransform(b->handle, w.position, w.rotation);
}
bool Scene::raycast(const glm::vec3& o, const glm::vec3& d, float maxDistance, SceneRayHit& out) const {
    physics::RayHit hit;
    if (!backend_ || !backend_->raycast(o, d, maxDistance, hit)) return false;
    auto it = handleToEntity_.find(hit.body);
    if (it == handleToEntity_.end()) return false;
    out = {it->second, hit.distance, hit.point, hit.normal};
    return true;
}
void Scene::drainCollisions(std::vector<SceneCollision>& out) {
    if (!backend_) return;
    std::vector<physics::ContactEvent> raw;
    backend_->drainEvents(raw);
    for (const auto& ev : raw) {
        auto a = handleToEntity_.find(ev.a), b = handleToEntity_.find(ev.b);
        if (a != handleToEntity_.end() && b != handleToEntity_.end()) out.push_back({a->second, b->second, ev.begin, ev.trigger});
    }
}

bool Scene::attachPhysicsBody(uint32_t entityId, const physics::BodyDesc& desc) {
    Entity* e = find(entityId);
    if (!e || registry_.get<PhysicsBody>(entityId)) return false;
    PhysicsComponent& c = e->physics;
    c.enabled = true; c.shape = physics::ShapeType::Box; c.autoFit = false; c.halfExtents = desc.halfExtents;
    c.motion = desc.isStatic ? physics::MotionType::Static : physics::MotionType::Dynamic;
    c.mass = desc.mass; c.friction = desc.friction; c.restitution = desc.restitution;
    c.linearDamping = desc.linearDamping; c.gravityScale = desc.useGravity ? desc.gravityScale : 0.0f;
    if (!backend_) { if (!startPhysics()) return false; }
    else if (!createRuntimeBody(*e)) return false;
    if (glm::dot(desc.velocity, desc.velocity) > 0.0f) setLinearVelocity(entityId, desc.velocity);
    return true;
}

bool Scene::detachPhysicsBody(uint32_t entityId) {
    if (!registry_.get<PhysicsBody>(entityId)) return false;
    destroyRuntimeBody(entityId);
    if (Entity* e = find(entityId)) e->physics.enabled = false;
    return true;
}

void Scene::updateOrbits(float t) {
    for (Entity& e : entities) {
        if (!e.orbit) continue;
        const float a = t * e.orbitSpeed + e.orbitPhase;
        e.position = e.orbitCenter + glm::vec3(std::cos(a) * e.orbitRadius, 0.0f, std::sin(a) * e.orbitRadius * 0.5f);
    }
}

} // namespace anvil
