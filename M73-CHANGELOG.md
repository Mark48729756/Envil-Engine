# M73 — Skeletal animation foundations

- Added `Animation.h`: validated parent-before-child skeletons, keyframe sampling, translation/scale interpolation, quaternion slerp, clip playback and two-bone planar IK.
- Added `shaders/rhi/skinning.vert`: four-influence linear blend skinning from a GPU bone palette.
- glTF animation import, global bone-matrix hierarchy evaluation, animation graphs/state machines, root motion, retargeting and GPU palette upload remain future integration work.
