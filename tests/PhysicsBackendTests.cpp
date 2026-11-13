// M32: backend-level tests. Runs against the Simple backend always and against Jolt when ANVIL_WITH_JOLT is on.
// Uses CHECK (not assert) so the tests still verify something in Release builds.
#include "anvil/physics/PhysicsBackend.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

static int g_failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++g_failures; } } while (0)

using namespace anvil::physics;

static std::uint32_t addFloor(IPhysicsBackend& w) {
    RigidBodyDesc f; f.motion = MotionType::Static; f.halfExtents = {20, 0.5f, 20}; f.position = {0, -0.5f, 0};   // top surface y = 0
    return w.createBody(f);
}
static void run(IPhysicsBackend& w, float seconds) { for (int i = 0; i < int(seconds * 60); ++i) w.step(1.0f / 60.0f); }

static void testBoxRests(IPhysicsBackend& w) {
    addFloor(w);
    RigidBodyDesc b; b.halfExtents = glm::vec3(0.5f); b.position = {0, 3, 0};
    auto h = w.createBody(b); CHECK(h != 0);
    run(w, 3.0f);
    BodyState s; CHECK(w.getState(h, s));
    CHECK(std::abs(s.position.y - 0.5f) < 0.06f);              // resting on the floor surface
    CHECK(glm::length(s.linearVelocity) < 0.2f);
    CHECK(std::isfinite(s.position.x) && std::isfinite(s.position.z));
}
static void testSphereAndCapsule(IPhysicsBackend& w) {
    addFloor(w);
    RigidBodyDesc s; s.shape = ShapeType::Sphere; s.radius = 0.5f; s.position = {-2, 2, 0};
    RigidBodyDesc c; c.shape = ShapeType::Capsule; c.radius = 0.3f; c.halfHeight = 0.5f; c.position = {2, 2, 0};
    auto hs = w.createBody(s), hc = w.createBody(c); CHECK(hs && hc);
    run(w, 3.0f);
    BodyState a, b; w.getState(hs, a); w.getState(hc, b);
    CHECK(std::abs(a.position.y - 0.5f) < 0.1f);               // sphere radius 0.5
    if (w.supportsRotation()) CHECK(std::abs(b.position.y - 0.8f) < 0.1f);   // capsule total half height 0.8
}
static void testRaycast(IPhysicsBackend& w) {
    auto floor = addFloor(w);
    RigidBodyDesc b; b.halfExtents = glm::vec3(0.5f); b.position = {5, 0.5f, 0}; b.motion = MotionType::Static;
    auto box = w.createBody(b);
    RayHit hit;
    CHECK(w.raycast({0, 10, 0}, {0, -1, 0}, 50.0f, hit));
    CHECK(hit.body == floor && std::abs(hit.distance - 10.0f) < 0.01f && hit.normal.y > 0.9f);
    CHECK(w.raycast({5, 10, 0}, {0, -1, 0}, 50.0f, hit));
    CHECK(hit.body == box && std::abs(hit.distance - 9.0f) < 0.01f);
    CHECK(!w.raycast({0, 10, 0}, {0, -1, 0}, 5.0f, hit));       // too short
    CHECK(!w.raycast({0, 10, 0}, {0, 0, 0}, 50.0f, hit));       // zero direction rejected
}
static void testImpulseAndVelocity(IPhysicsBackend& w) {
    RigidBodyDesc b; b.position = {0, 10, 0}; b.mass = 2.0f; b.gravityScale = 0.0f; b.linearDamping = 0.0f;
    auto h = w.createBody(b);
    CHECK(w.addImpulse(h, {4, 0, 0}));
    BodyState s; w.getState(h, s);
    CHECK(std::abs(s.linearVelocity.x - 2.0f) < 0.01f);         // J / m
    CHECK(w.setLinearVelocity(h, {0, 0, 3}));
    run(w, 1.0f); w.getState(h, s);
    CHECK(std::abs(s.position.z - 3.0f) < 0.1f);
    CHECK(!w.addImpulse(9999, {1, 0, 0}));
    CHECK(!w.setLinearVelocity(h, {NAN, 0, 0}));                 // non-finite input rejected
}
static void testKinematicAndTeleport(IPhysicsBackend& w) {
    RigidBodyDesc k; k.motion = MotionType::Kinematic; k.position = {0, 1, 0};
    auto h = w.createBody(k);
    CHECK(w.setTransform(h, {3, 1, 0}, glm::quat(1, 0, 0, 0)));
    BodyState s; w.getState(h, s); CHECK(std::abs(s.position.x - 3.0f) < 0.001f);
    run(w, 0.5f); w.getState(h, s); CHECK(std::abs(s.position.y - 1.0f) < 0.001f);   // kinematic ignores gravity
}
static void testDestroy(IPhysicsBackend& w) {
    auto a = addFloor(w); RigidBodyDesc b; auto c = w.createBody(b);
    CHECK(w.bodyCount() == 2);
    CHECK(w.destroyBody(c)); CHECK(!w.destroyBody(c)); CHECK(w.bodyCount() == 1);
    BodyState s; CHECK(!w.getState(c, s)); CHECK(w.getState(a, s));
    run(w, 0.5f);
}
// Jolt-only behaviour --------------------------------------------------------------------------
static void testRotation(IPhysicsBackend& w) {
    addFloor(w);
    RigidBodyDesc b; b.halfExtents = glm::vec3(0.5f); b.position = {0, 3, 0};
    b.rotation = glm::angleAxis(0.6f, glm::normalize(glm::vec3(1, 0, 1)));   // tilted: lands on an edge and tumbles
    auto h = w.createBody(b); BodyState s0; w.getState(h, s0);
    run(w, 1.0f);                                                // ~0.7 s of free fall, then the tilted box hits the floor and tumbles
    BodyState s1; w.getState(h, s1);
    CHECK(glm::dot(s0.rotation, s1.rotation) < 0.9999f);          // orientation actually changed
    run(w, 4.0f); w.getState(h, s1);
    CHECK(s1.position.y > 0.45f && s1.position.y < 0.6f);        // settled flat-ish on a face
    CHECK(std::abs(glm::length(s1.rotation) - 1.0f) < 1e-3f);    // unit quaternion
}
static void testSphereRolls(IPhysicsBackend& w) {
    addFloor(w);
    RigidBodyDesc s; s.shape = ShapeType::Sphere; s.radius = 0.5f; s.position = {0, 0.5f, 0}; s.linearVelocity = {4, 0, 0}; s.friction = 0.8f;
    auto h = w.createBody(s); run(w, 1.0f);
    BodyState st; w.getState(h, st);
    CHECK(std::abs(st.angularVelocity.z) > 1.0f);                // friction spins the ball about Z while it moves along +X
    CHECK(st.position.x > 1.0f);
}
static void testEvents(IPhysicsBackend& w) {
    addFloor(w);
    RigidBodyDesc trig; trig.motion = MotionType::Static; trig.isTrigger = true; trig.halfExtents = {1, 1, 1}; trig.position = {0, 1.5f, 0};
    auto t = w.createBody(trig);
    RigidBodyDesc b; b.halfExtents = glm::vec3(0.25f); b.position = {0, 6, 0};
    auto h = w.createBody(b);
    run(w, 2.0f);
    std::vector<ContactEvent> ev; w.drainEvents(ev);
    bool floorTouch = false, triggerEnter = false;
    for (auto& e : ev) { if (e.begin && !e.trigger && (e.a == h || e.b == h)) floorTouch = true; if (e.begin && e.trigger && ((e.a == t && e.b == h) || (e.a == h && e.b == t))) triggerEnter = true; }
    CHECK(floorTouch); CHECK(triggerEnter);
    std::vector<ContactEvent> again; w.drainEvents(again); CHECK(again.empty());   // drained
}
static void testCCD(IPhysicsBackend& w) {
    RigidBodyDesc wall; wall.motion = MotionType::Static; wall.halfExtents = {0.05f, 5, 5}; wall.position = {10, 0, 0};
    w.createBody(wall);
    RigidBodyDesc bullet; bullet.shape = ShapeType::Sphere; bullet.radius = 0.1f; bullet.gravityScale = 0; bullet.position = {0, 0, 0};
    bullet.linearVelocity = {400, 0, 0}; bullet.continuous = true; bullet.linearDamping = 0;
    auto h = w.createBody(bullet); run(w, 0.5f);
    BodyState s; w.getState(h, s); CHECK(s.position.x < 10.2f);   // 400 m/s would be 6.7 m/frame; CCD stops it at the wall
}

template <class F> static void each(const char* name, F&& fn, BackendKind kind) {
    auto w = createBackend(kind);
    std::printf("[%s] %s\n", w->name(), name);
    fn(*w);
}

int main() {
    BackendKind kinds[2] = {BackendKind::Simple, BackendKind::Jolt};
    for (BackendKind k : kinds) {
        if (k == BackendKind::Jolt && !joltAvailable()) { std::printf("Jolt not compiled in (-DANVIL_WITH_JOLT=ON): skipping Jolt tests\n"); continue; }
        each("box rests on floor", testBoxRests, k);
        each("sphere + capsule", testSphereAndCapsule, k);
        each("raycast", testRaycast, k);
        each("impulse / velocity / bad input", testImpulseAndVelocity, k);
        each("kinematic + teleport", testKinematicAndTeleport, k);
        each("destroy", testDestroy, k);
        if (k == BackendKind::Jolt) {
            each("rotation / tumbling", testRotation, k);
            each("rolling sphere angular velocity", testSphereRolls, k);
            each("contact + trigger events", testEvents, k);
            each("continuous collision (CCD)", testCCD, k);
        }
    }
    if (g_failures) { std::printf("%d CHECK(s) FAILED\n", g_failures); return 1; }
    std::printf("All physics backend tests passed\n");
    return 0;
}
