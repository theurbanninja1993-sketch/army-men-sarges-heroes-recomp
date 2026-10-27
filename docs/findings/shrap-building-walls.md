# Shrap interior wall static (2026-10-22)

## Reproduction and cause

The user confirmed the RMB aim fix, then supplied the two newest captures of a
building in Shrap (capture-000/001, 2026-10-22 12:01, guest frames 8809/8910).
Both have matching current/previous display lists and native/interpolated images.
The wooden interior is correct in the native images. Inserted frames show the
exterior metal wall coming through, creating the reported static-like flashing.
This is geometry overlap, not corrupt texture data.

GPU replay reproduces the defect directly from the user's TA pairs. A partially
matched large wall strip retains some native corners while shared-corner repair
moves other corners with matched neighbours. This bends a formerly planar panel
across its interior lining. In capture-001 at guest screen coordinate (550,160),
the native wood surface (TCW 0x880c41f4, strip 699) has reciprocal depth 3.0283;
the exterior metal strip (TCW 0x880c01ec, strip 706) is behind it at 3.0024.
After the old interpolation, the metal panel's depth there becomes 3.0594,
putting it in front of the wood. These IDs were diagnostic only.

## Correction

`patches/dreamcomp-interpolation-planar-panels.patch` applies to the engine after
the existing interpolation fixes, including snow-corner welding. The patch covers
`engine/render/src/interpolate.cpp` and its API comment header.

- Classify natively planar 3D strips using reciprocal-depth planes over projected
  X/Y. Use the largest native triangle as the fitting basis to avoid tiny areas.
- Recheck those planes after interpolation and shared-corner repair. Nonplanar
  native terrain strips and flat HUD/sprites retain their existing rules.
- Bent panels restore their corners to canonical native positions, together with
  every shared copy. The same work queue handles triangle folds and revisits
  neighbouring constraints after a restore, so wall repairs cannot open floor
  seams. Each shared corner can be restored only once, bounding the work.
- Native planarity tolerance is relative 1e-5, interpolated tolerance 1e-4, with
  an absolute 1e-7 reciprocal-depth floor. No game-specific material IDs or biases.

This is a local safety fallback for uncertain geometry correspondence. Affected
panels can use the native frame position when interpolation is unsafe; fully
matched planar panels continue to interpolate. Native rendering and texture
sampling are unchanged.

## Verification

- Both exact captured pairs replay with the wooden walls intact and no new gaps
  at adjoining floor/ceiling edges. Replay reports zero split shared vertices.
- The added planar-panel regression fails against the previous engine at the
  unsafe-corner assertion and passes with the correction. Tests also cover
  duplicate shared corners during rollback, fully matched panel interpolation,
  deliberately nonplanar terrain, and the previous snow one-ULP, depth-layer,
  flat HUD, folded triangle, correspondence, camera-cut and projection cases.
- The playable executable, capture replayer and interpolation test target were
  rebuilt. Engine/port git diff checks and saved patch reverse-apply checks pass.
- A 100-blend CPU benchmark of the 7591-vertex capture was run with the same
  compiler/optimization settings before and after; results were a few milliseconds
  per blend. Temporary diagnostics and benchmark binaries remain under /tmp.

Images: `shots/shrap-building-walls/`. Capture-001 before/after are matched GPU
replays; capture-000 before is the user's interpolated image resized for viewing,
and its after is a GPU replay. Raw source fixtures are preserved under
`/tmp/army-shrap-building-captures/`; the user's originals were not modified.
GPU intermediate images and diagnostics use `/tmp/army-building-*` paths.
Gameplay confirmation after restarting remains pending.

## Follow-up: rapid camera reversals and crossing layers (2026-10-22)

The user reported the first fix was insufficient when moving the mouse back and
forth, and supplied another capture sequence at 12:15. Capture-001 through 005
have valid paired interpolation metadata. Capture-000 says no paired capture;
its older interpolation companions are stale and were excluded from verification.

The new frames expose a second failure: individually planar inner and outer wall
layers can cross when one interpolates and the other stays native. At guest
coordinate (200,160) in capture-003, the native inner wood depth is 1.74867 and
outer metal 1.73611. The first fix leaves the wood planar but moves it to 1.67766,
behind the unchanged metal. Native plane normals agree to about 1e-7, confirming
nearby parallel layers; panel planarity alone cannot protect their ordering.

The additional `patches/dreamcomp-interpolation-wall-layers.patch` applies after
the planar-panel patch:

- Collect visible natively planar opaque/punch-through world surfaces. A 64-pixel
  screen grid finds nearby overlap candidates and suppresses duplicate comparisons.
- Consider only nearly parallel, separated layers (normalized-plane dot product
  at least 1-1e-7, reciprocal-depth separation below 5%). Coplanar neighbours and
  genuinely intersecting/nonparallel faces do not create a layer constraint.
- If their interpolated depth planes reverse the native ordering over the shared
  screen bounding rectangle, restore both surfaces and every shared corner copy.
  The existing iterative queue rechecks adjoining surfaces and layers, so this
  cannot open a crack between a restored wall and its floor.
- Layer tests ignore extrapolated points behind the projection plane. Coherent
  movement of both layers remains interpolated. This is a conservative local
  fallback; it does not alter native geometry, texture data, or visibility/culling.

Verification:

- All five valid new pairs were replayed on the GPU at t=.25, .5, .75 and with
  frame order reversed: 20 replays. The exterior wall bleed is absent in the
  examined sequence, and all report zero split shared vertices. The original two
  pairs also retain their correction. Inputs are preserved under
  /tmp/army-building-followup-captures/; results and sheet under
  /tmp/army-building-followup-replays/.
- The new layered-wall unit test fails against the planar-only implementation,
  then passes with the new guard. It covers both an interpolated front layer and
  interpolated back layer, while a coherent two-layer move remains interpolated.
  Existing planar-panel, nonplanar terrain, snow corners, shared seams, HUD,
  correspondence, projection and camera-cut regressions pass.
- Capture replay now accepts --blend-t FRACTION (0..1), and reports native-frame
  fallback explicitly on camera cuts. It remains under build/tools.
- A 100-blend benchmark with the same compiler/optimization on the 10522-vertex
  new capture measures about 4.23 ms before and 4.73 ms after the layer guard.
- The game, replay tool and interpolation tests are rebuilt; engine/port diff
  checks and isolated patch reverse-apply checks pass. No capture originals or
  user settings/cards were changed. Live back-and-forth gameplay confirmation
  remains pending, superseding the first correction's earlier confirmation status.

Matched GPU images: shots/shrap-building-walls/followup-003-{before,fixed,reverse}.png.
