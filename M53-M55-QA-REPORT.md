# M53–M55 QA Report

## Changes
- M53: RHI mesh/albedo cache now tracks estimated bytes and performs budget-aware LRU eviction; current-frame resources remain pinned by shared ownership.
- M54: OpenGL post-processing now has optional temporal accumulation with depth-based disocclusion rejection, neighborhood clipping and persistent history targets.
- M55: Added portable source-contract coverage and editor controls for temporal AA.

## Validation
- Portable `ANVIL_ASSET_TESTS_ONLY=ON` build and CTest were run for this package; see actual result in the delivery report.
- ZIP integrity was checked after packaging.

## Limitations
- No GLSL compiler or GPU execution is assumed by the source-contract test.
- TAA currently handles camera reprojection for static surfaces; moving-object velocity is not implemented and can cause ghosting if TAA is enabled on animated objects.
- RHI TAA is not implemented; the temporal resolve is currently on the OpenGL renderer path.
- Cache byte accounting is an estimate and excludes driver allocation/alignment overhead. A hard GPU budget cannot be guaranteed while resources are referenced by in-flight frames.
