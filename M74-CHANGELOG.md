# M74 — Character Controller
- Added a deterministic kinematic character movement core with acceleration, normalized movement input, gravity, jumping, grounded state and configurable slope acceptance.
- Collision queries and actual capsule sweeps remain host-physics responsibilities; this module does not claim to perform collision detection itself.
