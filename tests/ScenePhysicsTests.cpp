// M32: Scene / ECS / serializer physics integration. No window or GL context is needed.
#include "anvil/Scene.h"
#include "anvil/SceneSerializer.h"
#include <cmath>
#include <cstdio>

static int g_failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++g_failures; } } while (0)

using namespace anvil;

static Entity& makeGround(Scene& s) {
    Entity& g = s.add("Ground"); g.primitive = Primitive::Plane;
    g.physics.enabled = true; g.physics.motion = physics::MotionType::Static;
    return g;
}
static void run(Scene& s, float sec) { for (int i = 0; i < int(sec * 60); ++i) s.stepPhysics(1.0f / 60.0f); }

int main() {
    std::printf("physics backend: %s\n", physics::createBackend()->name());

    // --- ECS registry (M32 changes) ---
    { ecs::Registry r;
      CHECK(r.createWithId(7)); CHECK(!r.createWithId(7)); CHECK(!r.createWithId(0)); CHECK(r.valid(7));
      CHECK(r.create() == 8);                       // generated ids continue after explicit ones
      r.emplace<int>(7, 42); CHECK(*r.get<int>(7) == 42);
      r.destroy(7); CHECK(!r.valid(7)); CHECK(r.get<int>(7) == nullptr);
      CHECK(r.view<int>().empty()); }

    // --- Scene: authoring data -> ECS runtime -> backend ---
    Scene s;
    Entity& ground = makeGround(s); const uint32_t groundId = ground.id;
    Entity& ball = s.add("Ball"); ball.primitive = Primitive::Sphere; ball.position = {0, 5, 0};
    ball.physics.enabled = true; ball.physics.restitution = 0.1f; const uint32_t ballId = ball.id;
    Entity& plain = s.add("NoPhysics"); plain.primitive = Primitive::Cube; plain.position = {3, 5, 0}; const uint32_t plainId = plain.id;
    Entity& parent = s.add("Parent"); parent.position = {10, 0, 0}; parent.rotation = {0, 90, 0}; const uint32_t parentId = parent.id;
    Entity& child = s.add("Child"); child.primitive = Primitive::Cube; child.parentId = parentId; child.position = {0, 4, 0};
    child.physics.enabled = true; const uint32_t childId = child.id;

    CHECK(!s.physicsRunning());
    CHECK(s.startPhysics());
    CHECK(s.physicsRunning());
    CHECK(s.physicsBodyCount() == 3);                              // ground, ball, child; 'plain' and 'parent' have no Rigidbody
    CHECK(s.registry().get<PhysicsBody>(ballId) != nullptr);
    CHECK(s.registry().get<PhysicsBody>(plainId) == nullptr);
    CHECK(s.registry().view<PhysicsBody>().size() == 3);

    run(s, 4.0f);
    const Entity* b = s.find(ballId);
    CHECK(std::abs(b->position.y - 1.0f) < 0.1f);                  // sphere radius 1 resting on the plane (top at y = 0)
    CHECK(std::abs(b->position.x) < 0.05f);
    const Entity* c = s.find(childId);                             // parented body: simulated in world space, written back in parent space
    CHECK(std::abs(c->position.y - 1.0f) < 0.1f);                  // local y == world y (parent only rotates around Y)
    CHECK(std::abs(c->position.x) < 0.1f && std::abs(c->position.z) < 0.1f);
    CHECK(s.find(plainId)->position.y == 5.0f);                    // untouched

    // raycast / impulse / velocity ---------------------------------------------------------
    SceneRayHit hit;
    CHECK(s.raycast({30, 10, 30}, {0, -1, 0}, 50.0f, hit));
    CHECK(hit.entity == groundId && std::abs(hit.distance - 10.0f) < 0.01f && hit.normal.y > 0.9f);
    CHECK(s.raycast({0, 10, 0}, {0, -1, 0}, 50.0f, hit) && hit.entity == ballId);
    CHECK(s.addImpulse(ballId, {0, 20, 0}));
    glm::vec3 v; CHECK(s.getLinearVelocity(ballId, v) && v.y > 15.0f);
    CHECK(!s.addImpulse(plainId, {0, 1, 0}));                      // no body
    run(s, 0.5f); CHECK(s.find(ballId)->position.y > 2.0f);        // launched

    // remove while running -------------------------------------------------------------------
    s.remove(ballId);
    CHECK(s.physicsBodyCount() == 2); CHECK(!s.registry().valid(ballId)); CHECK(s.find(ballId) == nullptr);
    run(s, 0.2f);

    // reparent while running keeps the world pose ---------------------------------------------
    const glm::vec3 beforeWorld = glm::vec3(s.worldModel(childId)[3]);
    CHECK(s.setParent(childId, 0));
    s.find(childId)->position = beforeWorld;                        // the editor keeps local values; emulate world-preserving reparent
    CHECK(s.teleportBody(childId));

    // stop: bodies gone, authoring data + simulated transforms stay ----------------------------
    const float groundY = s.find(groundId)->position.y;
    s.stopPhysics();
    CHECK(!s.physicsRunning() && s.physicsBodyCount() == 0 && s.registry().view<PhysicsBody>().empty());
    CHECK(s.find(childId)->physics.enabled && s.find(groundId)->position.y == groundY);

    // kinematic platform follows the entity ---------------------------------------------------
    { Scene k; Entity& plat = k.add("Platform"); plat.primitive = Primitive::Cube; plat.scale = {2, 0.2f, 2};
      plat.physics.enabled = true; plat.physics.motion = physics::MotionType::Kinematic; const uint32_t pid = plat.id;
      CHECK(k.startPhysics());
      k.find(pid)->position = {20, 0, 0}; run(k, 0.3f);
      SceneRayHit h2; const bool rayOk = k.raycast({20, 10, 0}, {0, -1, 0}, 50.0f, h2);
      CHECK(rayOk && h2.entity == pid);
      if (physics::joltAvailable()) CHECK(rayOk && std::abs(h2.distance - 9.8f) < 0.05f); }

    // legacy M26 API still works ----------------------------------------------------------------
    { Scene l; Entity& e = l.add("Legacy"); e.position = {0, 3, 0}; const uint32_t id = e.id;
      physics::BodyDesc d; d.halfExtents = glm::vec3(0.5f); d.mass = 1;
      CHECK(l.attachPhysicsBody(id, d)); CHECK(!l.attachPhysicsBody(id, d));
      run(l, 0.5f); CHECK(l.find(id)->position.y < 3.0f);          // falls under gravity
      CHECK(l.detachPhysicsBody(id)); CHECK(!l.detachPhysicsBody(id)); CHECK(!l.find(id)->physics.enabled); }

    // serialization v5 round trip ---------------------------------------------------------------
    { Scene a; Entity& e = a.add("Crate"); e.primitive = Primitive::Cube; auto& p = e.physics;
      p.enabled = true; p.shape = physics::ShapeType::Capsule; p.motion = physics::MotionType::Kinematic; p.autoFit = false;
      p.halfExtents = {0.5f, 1.5f, 2.5f}; p.radius = 0.4f; p.halfHeight = 0.9f; p.mass = 12.5f; p.friction = 0.3f; p.restitution = 0.7f;
      p.linearDamping = 0.1f; p.angularDamping = 0.2f; p.gravityScale = 0.5f; p.isTrigger = true; p.continuous = true;
      a.add("Plain");
      const std::string json = SceneSerializer::toJson(a);
      CHECK(json.find("\"version\":5") != std::string::npos);
      Scene bScene; std::string err; CHECK(SceneSerializer::fromJson(bScene, json, &err));
      const auto& q = bScene.entities[0].physics;
      CHECK(q.enabled && q.shape == physics::ShapeType::Capsule && q.motion == physics::MotionType::Kinematic && !q.autoFit);
      CHECK(q.halfExtents == glm::vec3(0.5f, 1.5f, 2.5f) && q.radius == 0.4f && q.halfHeight == 0.9f);
      CHECK(q.mass == 12.5f && q.friction == 0.3f && q.restitution == 0.7f && q.linearDamping == 0.1f && q.angularDamping == 0.2f);
      CHECK(q.gravityScale == 0.5f && q.isTrigger && q.continuous);
      CHECK(!bScene.entities[1].physics.enabled);
      // older documents load with physics disabled; unknown future versions are rejected
      std::string v4 = json; v4.replace(v4.find("\"version\":5"), 11, "\"version\":4");
      Scene oldScene; CHECK(SceneSerializer::fromJson(oldScene, v4, &err)); CHECK(!oldScene.entities[0].physics.enabled);
      std::string v6 = json; v6.replace(v6.find("\"version\":5"), 11, "\"version\":6");
      Scene futureScene; CHECK(!SceneSerializer::fromJson(futureScene, v6, &err));
      // loading into a running scene tears the old bodies down first
      Scene running; makeGround(running); CHECK(running.startPhysics() && running.physicsBodyCount() == 1);
      CHECK(SceneSerializer::fromJson(running, json, &err)); CHECK(!running.physicsRunning()); }

    // garbage in the file must not produce NaN bodies ---------------------------------------------
    { Scene a; Entity& e = a.add("Bad"); e.primitive = Primitive::Cube; e.physics.enabled = true; e.physics.mass = -5.0f;
      e.position = {0, 2, 0}; CHECK(a.startPhysics()); run(a, 1.0f); CHECK(std::isfinite(a.find(e.id)->position.y)); }

    if (g_failures) { std::printf("%d CHECK(s) FAILED\n", g_failures); return 1; }
    std::printf("All Scene/ECS physics tests passed\n");
    return 0;
}
