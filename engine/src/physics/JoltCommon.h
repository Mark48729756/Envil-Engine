#pragma once
// Internal: process-wide Jolt registration and the layer setup shared by the Jolt adapters.
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <mutex>

namespace anvil::physics::jolt {

inline void ensureRegistered() {
    static std::once_flag once;   // inline function: one flag shared by every translation unit
    std::call_once(once, [] {
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
    });
}

// Base-class trick: list this FIRST in the base list so Jolt's allocator is registered before any other
// member (TempAllocatorImpl, PhysicsSystem...) allocates. Registering inside the constructor body is too late.
struct RegisterFirst { RegisterFirst() { ensureRegistered(); } };

constexpr JPH::ObjectLayer kStaticLayer = 0;
constexpr JPH::ObjectLayer kMovingLayer = 1;
constexpr JPH::BroadPhaseLayer kStaticBroadPhase(0);
constexpr JPH::BroadPhaseLayer kMovingBroadPhase(1);

class BroadPhaseLayers final : public JPH::BroadPhaseLayerInterface {
public:
    JPH::uint GetNumBroadPhaseLayers() const override { return 2; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer l) const override { return l == kStaticLayer ? kStaticBroadPhase : kMovingBroadPhase; }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer l) const override { return l == kStaticBroadPhase ? "Static" : "Moving"; }
#endif
};
class ObjectVsBroadPhase final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer o, JPH::BroadPhaseLayer b) const override { return o == kMovingLayer || b == kMovingBroadPhase; }
};
class ObjectPairs final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override { return a == kMovingLayer || b == kMovingLayer; }
};

} // namespace anvil::physics::jolt
