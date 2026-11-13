# ANVIL Engine — D3D12 Compute QA report (M45)

## Defect found in M44
The output buffer was zero-initialized, while the validation formula assumed it contained the index `i`. This was a test-oracle defect: the expected output did not correspond to the bytes uploaded before the first dispatch.

## Correction
The upload staging buffer is now initialized with `0, 1, 2, ... 127`. The direct dispatch covers indices 0–63; the indirect dispatch covers 0–127. With the shader operation `v = v * 3 + 7`, the expected results are:

- `i < 64`: `(i * 3 + 7) * 3 + 7`
- `i >= 64`: `i * 3 + 7`

The portable contract test mirrors this exact sequence and verifies all 128 outputs.

## Reliability changes
- Fence waits fail on device removal (`GetCompletedValue() == UINT64_MAX`).
- Event registration and wait return values are checked.
- Missing hardware adapter falls back to the D3D12 WARP adapter; test skips only if neither hardware nor WARP is available.
- Existing UAV ordering remains explicit: UAV barrier between the direct and indirect writes, another UAV barrier before transition to `COPY_SOURCE`, then readback.

## Results
- Portable build: PASS
- Portable tests: 6/6 PASS
- Native D3D12 GPU validation: NOT RUN (host is not Windows)

This report deliberately separates portable test results from native GPU validation; it does not claim a D3D12 GPU pass.
