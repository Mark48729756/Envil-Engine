# M61 — Asynchronous asset streaming

- Added `AsyncAssetStreamer`: one background IO worker, priority-ordered requests, duplicate suppression while pending/active, completion polling and explicit shutdown.
- Worker reads file bytes only; GPU upload and resource creation are intentionally left to the render thread.
- Added deterministic tests using a temporary file.
- Scene cell streaming, eviction policy and renderer upload integration remain future work.
