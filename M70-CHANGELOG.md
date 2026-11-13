# M70 — Dynamic weather foundations

- Added `Weather.h`: deterministic weather-state transitions, wind interpolation, rain wetness accumulation/drying and temperature-dependent snow accumulation/melt.
- Added `shaders/rhi/weather_particles.comp`: compute prototype for wind-advected precipitation particles with bounded lifetime and deterministic respawn.
- Particle buffer allocation, dispatch scheduling, precipitation draw pass, material wetness/snow bindings and visual GPU validation remain pending.
