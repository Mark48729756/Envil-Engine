# M62 — GPU-driven indirect draw preparation

- Added a Vulkan GLSL compute pass that maps per-object visibility flags to indexed-indirect commands.
- Hidden objects receive `instanceCount = 0`; visible commands preserve their draw arguments.
- This is a command-preparation pass, not yet a renderer dispatch: descriptor allocation, barriers, command-buffer dispatch and `vkCmdDrawIndexedIndirect` submission remain integration work.
