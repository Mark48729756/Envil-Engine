# M77 — Spatial Audio Core
- Added a dependency-free spatial audio scene model with listener and emitter state.
- Added distance attenuation, stereo pan, radial-velocity Doppler approximation, and occlusion-driven gain/low-pass parameters.
- Added emitter registration, playback state, lookup, and removal.
- This is a mixer-facing calculation layer; it does not yet decode audio files or submit samples to an OS audio device.
