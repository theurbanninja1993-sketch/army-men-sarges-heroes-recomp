# Portal layers changing with view angle (2026-10-22)

## Reproduction

The user supplied capture-000 (bad angle, 2026-10-21 19:25, guest frame 48428)
and capture-001 (correct appearance, 19:26, guest frame 48771). Both include a
native and interpolated image plus the corresponding source lists. The defect
is already visible in the native frame: the yellow center is lost and scenery
shows prominently through part of the blue portal at the first angle.

The portal consists of 12 translucent strips using TCW `0x900C5C6C` in these
captures. The back disk and the yellow/blue foreground layer have overlapping
screen bounds and depth ranges. The renderer's minimum-reciprocal-depth key
sorts whole strips, which interleaves these layers incorrectly at oblique angles.
A controlled replay putting the foreground layer after the back disk restores
the center. The texture address/color classification was used only for this
diagnostic; the shipping correction contains no portal-specific texture IDs.

## Correction

Applied shared-engine patch:
`patches/dreamcomp-translucent-overlap-sort.patch`.

- New device-independent `dream/render/translucent_sort.h` and
  `src/translucent_sort.cpp` refine the existing coarse back-to-front order.
- Compute each strip triangle's reciprocal-depth plane, intersect pairs of
  projected triangles, and compare depth over the actual overlap. Bounding-box
  overlap alone does not establish a draw-order relation.
- Consistently separated surfaces create back-before-front dependencies. A stable
  topological ordering resolves those relations while retaining the original
  priority for unconstrained faces. Every triangle of a strip is considered.
- Touching edges, coplanar pairs, and genuinely intersecting surfaces create no
  ordering constraint. Cycles use the original priority as a deterministic
  fallback; faces are never discarded. This is not full per-pixel transparency
  sorting, so intersecting transparency remains an approximation.
- The Vulkan renderer applies the refinement to auto-sorted translucent lists.
  The shaders, blending modes, textures, and opaque/punch-through pass ordering
  are unchanged. Both native and interpolated renders use the correction.

## Verification

- Rebuilt the game, capture replayer, and optional
  `armymensh_transparency_tests` target (executable under `build/tools/`).
- Final GPU replays of both captures, native and interpolated, restore the yellow
  center at the bad angle. The good-angle native replay is pixel-for-pixel
  identical to the previous renderer.
- At 1280x720, the bad-angle native replay changes 8,516 pixels, all within
  x=447..560, y=164..322 inside the portal. The rest of the scene is unchanged.
- Both interpolated replay checks still report zero split shared vertices.
- Regression cases pass for misleading minimum depth, correct-order stability,
  bounding boxes with no triangle overlap, touching edges, coplanar overlays,
  intersecting planes, alternating strip winding, and multiple separated layers.
- A 1,000-iteration CPU benchmark of each captured scene (42 translucent strips)
  measures roughly 0.022 ms per overlap refinement.
- `git diff --check` is clean in both checkouts, and the saved engine patch passes
  the reverse-apply check. Existing boot-source conversion/shadow warnings remain.

Final replay images: `/tmp/army-portal-final-{0,1}-{native,interpolated}.png`.
Baseline images: `/tmp/army-portal-{0,1}-base.png`.
Build log: `/tmp/army-portal-build.log`. Gameplay confirmation is still pending.
