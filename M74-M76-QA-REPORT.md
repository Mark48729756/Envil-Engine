# ANVIL Engine M74–M76 QA Report

## Included
- M74: header-only kinematic character controller core with acceleration, jump, gravity, grounded state and configurable slope acceptance. Host collision sweeps/ground queries are not implemented here.
- M75: animation state graph with named states, directed transitions, cross-fades and a 1D blend-tree sampler returning the two clips and interpolation alpha. Multi-clip final-pose evaluation is not yet integrated.
- M76: CPU distance-constraint ragdoll prototype with inverse-mass weighting, stiffness, bounded iterations and activation/blend controls. It is not a full rigid-body ragdoll and is not wired to the physics backend.

## Verification
- CMake configure: PASS (`ANVIL_ASSET_TESTS_ONLY=ON`, Debug)
- C++ test build: PASS
- CTest: 27/27 PASS
- ZIP integrity: verified separately after packaging

## Not verified
- Full editor/runtime build and interactive scene execution
- GPU execution, shader compilation or FPS
- Character collision capsule sweeps, animation graph editor integration, ragdoll collision/angular joint constraints
