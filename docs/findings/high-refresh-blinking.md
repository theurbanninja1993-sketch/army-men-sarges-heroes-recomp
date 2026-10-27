# High refresh rate dark blinking (2026-10-21)

The user reported faint dark blinking at high frame rates, more visible when
looking in some directions. The HUD did not appear affected. The monitor reports
180 Hz and the port uses automatic interpolation, with internal resolution 4x.

## Reproduced GPU errors

A 500-frame Vulkan validation run with interpolation enabled produced repeated
image-layout and descriptor-lifetime errors (20 messages before duplicate
suppression):

- `VUID-vkCmdDraw-None-09600`: the inserted frame was sampled as shader-readable
  while actually in `TRANSFER_SRC_OPTIMAL`. `interp_off` retained the default
  `readback=true`, which leaves its image ready for copying rather than sampling.
- `VUID-vkUpdateDescriptorSets-None-03047`: the native and inserted frames shared
  one `Presenter`. Each `draw_view` rewrote its external-image descriptor while
  an earlier presentation could still be reading it on the GPU.

Both are incorrect GPU usage in the high frame rate path and can cause image
corruption. Subsequent play testing still showed slight dark blinking and newly
noticeable tree/texture flashing. The user confirmed selecting 60 FPS stops both
symptoms; the initial correction is therefore not a confirmed symptom fix.

## Correction

`patches/dreamcomp-interpolation-present.patch` updates the shared runtime's
`engine/runtime/boot/boot_main.cpp`:

- Set `interp_off.readback=false` before creation so renders transition the
  inserted image to `SHADER_READ_ONLY_OPTIMAL`. Lazy screenshot readback still
  restores this layout after copying. The flag survives resolution/aspect rebuilds.
- Give inserted frames their own `Presenter`, keeping each source's image
  descriptor stable while presentations are in flight. Retargeting already waits
  for the device before replacing images.

The patch is applied to the engine checkout and the port executable is rebuilt.
Game logic, geometry interpolation, ground-seam correction, camera projection,
and the confirmed target-ring HUD exclusion are unchanged.

## Verification

- Identical 500-frame interpolated checks before/after: the two Vulkan errors
  disappear, from 20 messages to zero. Both runs draw 309 blended frames and
  present 308, so interpolation remains enabled.
- Native 60 FPS control, 500 frames at 4x: zero Vulkan messages.
- Campaign run, 8,500 frames at 4x expanded aspect with interpolation and camera
  input: zero Vulkan messages; 5,489 blended frames drawn, 5,488 presented.
- Two stationary indoor gameplay screenshot pairs at presented frames 8380 and
  8420: native versus inserted average luma differs by about 0.0001 on a 0–255
  scale. This verifies those captured views, not the user's outdoor symptom.

Diagnostics are in `/tmp/army-flicker-validation.log`,
`/tmp/army-flicker-fixed-validation.log`, and
`/tmp/army-flicker-gameplay-check/`. Captures remain outside git.

Short validation reproduction from the port directory (requires GPU/window access
and the installed Vulkan validation layer):

```sh
./build/game/army-men-sarges-heroes-recomp \
  --config game/armymensh.toml \
  --disc ../../disc/gdi/army-men-sarges-heroes.gdi \
  --window --no-audio --validation --unthrottled --present-mode immediate \
  --max-frames 500 --settings /tmp/army-flicker-empty.ini \
  --set fps=120 --scale 4 --aspect 16:9
```

## Follow-up: remaining scenery/texture flashing

Stricter synchronization validation (`VK_LAYER_VALIDATE_SYNC=1`, in addition to
`--validation`) exposed a shared window depth attachment write-after-write
hazard. All swapchain framebuffers use the same depth image, but the external
subpass dependency omitted late fragment tests and the previous depth write
access. `patches/dreamcomp-present-depth-sync.patch` adds those dependencies in
`Window::create_targets`. This fixes another objective GPU error; a relationship
to the reported scenery flashing has not been established.

After this change, a 400-frame interpolated 4x run has zero synchronization or
other GPU validation messages (209 frames blended, 208 inserted frames presented).
The old outdoor 60-render capture sequence was also replayed on the GPU, but it
did not reproduce the full reported symptom. Geometry matching has some UV
correspondence discrepancies, including clipped/animated surfaces; no matching
algorithm changes were made without a reproduction of the texture flashing.

The old F11 captures contain only the native image and current source display
list, so they omit the inserted image the user may see flashing. The applied
`patches/dreamcomp-interpolation-capture.patch` extends F11 automatically when a
current inserted frame exists, with no environment flags or timed RAM dumps:

- `capture-NNN.ppm`: native image, as before.
- `capture-NNN-interp.ppm`: corresponding inserted image.
- `capture-NNN.ta`: current display list, as before.
- `capture-NNN-previous.ta`: previous display list used to form that inserted image.
- Existing VRAM/register files remain alongside these; metadata records whether
  a paired interpolation capture was written.

An explicit current-frame flag prevents exporting an old inserted image after a
camera cut, resize, or skipped blend. The usual F12 interpolation screenshots
also use this guard. A short GPU run with `--capture-at 200` verified both images
are 3413x1920, both source lists are written, metadata reports a pair, and strict
validation remains clean. Test outputs: `/tmp/army-flicker-f11-check/`.

The user then supplied five paired F11 captures, described below. The user has
confirmed 60 FPS as a workaround for both the dark blinking and texture flashing.

## Reproduced tree collapse and geometry correspondence correction

The paired captures collected at 18:01 are `capture-000` through `capture-004`
(guest frames 5244, 5295, 5410, 5719, 5811). They include both native and inserted
images and the exact previous/current source display lists. Captures 000–001 are
nearly stationary; captures 002–004 contain camera movement.

GPU replay of these pairs reproduces trees with collapsed or doubled crowns,
wide stump-like trunks, and distorted rock faces in the inserted frame even
though both native endpoints are intact. This is a geometry interpolation
failure. Render state and vertex count alone let the matcher pair unrelated
faces that share a texture after visibility/clipping changes their submission
order. Blending their corners morphs parts of the scene into other geometry.

`patches/dreamcomp-interpolation-correspondence.patch` updates the engine matcher:

- Require matching per-vertex texture coordinates (tolerance 0.00001), so the same
  material cannot pair different atlas regions or changed clipped corners.
- Check every triangle in a strip for consistent winding and compatible area.
  For triangles exceeding one logical pixel in doubled area at both endpoints,
  reject opposite winding or an area change greater than 4x.
- Continue the existing lookahead to find a valid partner. Faces without a safe
  partner retain their native geometry; shared-corner repair remains active.

The checks are conservative: scrolling UVs or substantially changed clipping can
retain native cadence locally. Interpolation remains enabled for reliable faces.
No frame timing or game logic rates were changed by this correction.

Verification against the actual captured input pairs:

| Capture | Flipped/collapsed correspondences before | After |
| --- | ---: | ---: |
| 000 | 0 | 0 |
| 001 | 0 | 0 |
| 002 | 193 | 0 |
| 003 | 202 | 0 |
| 004 | 266 | 0 |

The diagnostic flags substantial winding changes and midpoint area collapse;
these counts include unsafe correspondences already skipped by the jump check.
The rendered before/after images also confirm the visible tree distortion is
removed. Final code still blends 3,476 strips in capture 002 and 2,192 in capture
004. CPU blending of the latter scene averages about 1.44 ms in a 100-iteration
check. The original 59 moving-ground pairs retain zero split shared vertices
(worst separation 0.000000 pixels).

`tools/test_interpolation.cpp` now checks mismatched texture corners, lookahead
past an inserted same-material face, opposite winding, and drastic area changes,
alongside the existing ground-corner, flat-HUD, and camera-cut cases. All pass.
The port executable and replay tool are rebuilt. Gameplay confirmation of the
remaining dark blinking and tree flashing is still needed after this change.

The replay tool now applies diagnostics/HUD correction to **both** native source
frames before blending, matching the live runtime order. Previously `--wide`
corrected only the final blended frame, which could obscure classifications and
did not reproduce the live input precisely. Example for the worst captured pair:

```sh
./build/tools/armymensh_render_capture capture-004.ta capture-004.vram \
  /tmp/army-004-production-fixed.png --previous capture-004-previous.ta \
  --wide --hud game/hud_overrides.ini
```

This reports zero split shared vertices, 2,192 blended strips, and 46 skipped
strips. Before/after GPU images and diagnostic logs are in
`/tmp/army-live-00{0,1,2,3,4}-*` and `/tmp/army-fixed-00{0,1,2,3,4}-*`.

## Follow-up captures: repeated objects and repair-induced folds

The user reported that the first correspondence correction reduced, but did not
eliminate, trees/logs jumping and dark blinking. Thirteen new paired F11 captures
(`capture-000` through `capture-012`, collected at 18:16) replace the earlier
numbered captures. Do not treat the earlier fix as a confirmed symptom fix.

Capture 009 reproduces another large tree mismatch: foliage using TCW
`0x800b8834` pairs with the same texture/UV pattern on a different tree. Example
strip 313 has reciprocal depths around 0.69–0.70 in the older source and
0.45 in the newer source, with roughly 51–64 logical pixels of displacement.
Several nearby faces exceed the existing 64-pixel jump guard while others do
not. Shared-corner repair then combines native and blended corners; at least
seven substantial foliage triangles collapse or reverse in the replay. Both
native endpoints have intact trees. The earlier endpoint winding/area tests are
insufficient for these repeated objects and for damage introduced after repair.

`patches/dreamcomp-interpolation-spatial.patch` is applied after the previous
correspondence patch and the game/replay binaries are rebuilt. It changes:

- Keep the ordered correspondence probe for camera-cut detection. For actual
  blending, bucket older faces by material/count/clip state and find the nearest
  compatible face across the list, independent of submission order or an
  eight-face resynchronization limit. Include relative reciprocal-depth movement
  in the cost, reject greater than 10% relative depth changes, and assign an older
  face to at most one newer face. Unsafe/ambiguous faces retain native geometry.
- Reject correspondences whose intermediate triangle collapses despite compatible
  endpoint winding/area.
- After shared-corner repair, check **all** resulting triangles. If a substantial
  face folds or collapses below 25% of its native area, restore its corner groups
  together with their shared copies and recheck affected neighbours. A work queue
  terminates because every corner group can be restored at most once. Restoration
  is local, so a small unsafe triangle does not freeze the entire connected ground
  mesh. Shared terrain vertices remain coincident.
- Blend positions before perspective projection, then project again, rather than
  independently averaging screen coordinates and reciprocal depth. This preserves
  planar surfaces under camera translation, including differently tessellated
  overlapping surfaces. A regression checks the expected projected camera-space
  position at four interpolation fractions. It is a correctness improvement for
  depth consistency; a connection to the user's remaining dark blinking has **not**
  been demonstrated in a captured frame.

Verification:

- All 13 new source pairs replay successfully on the GPU, with zero flagged
  folded/collapsed correspondences and zero Vulkan validation/synchronization
  errors. Capture 009 still blends 2,266 strips; its large tree deformation is
  removed in the replay. Outputs/logs: `/tmp/army-spatial-000` through `012`.
- A separate CPU check of all triangles after final repair found zero substantial
  folds across all 13 pairs. Most faces remain moving; local restoration avoids
  the earlier prototype's whole-ground fallback.
- All 59 earlier moving-ground pairs still have zero split shared vertices,
  worst separation 0.000000 pixels. Current captured-scene blending averages
  about 3.64 ms over 100 iterations, versus roughly 1.44 ms before spatial search.
- Unit regressions pass for the existing cases plus repeated faces at different
  depths, correspondence beyond the ordinal search window, choosing a closer
  repeated face even when the ordinal pair looks compatible, midpoint collapse,
  repair-induced folding, and perspective-correct planar movement.
- `git diff --check` is clean in the port and shared engine.

The first forced-validation offscreen harness run exposed its existing missing
instance-extension setup (`VK_KHR_surface` was not enabled while the context
requested `VK_KHR_swapchain`). The temporary replay harness now explicitly
requests the surface instance extension; all 13 scenes were rerun with validation
and sync validation enabled and no errors. This is test-harness setup, not a
change to the live window path.

Remaining: gameplay confirmation of the tree/log motion and especially the dark
blinking. These captures do not establish an entire dark/black presentation frame.
60 FPS remains the user-confirmed workaround; no FPS/settings changes were made.


## Gameplay confirmation: Adaptive Sync (2026-10-21)

After the spatial interpolation build, the user supplied six further paired
captures (000–005 at 18:35) while reporting intense black flashes and the old
in-game pause screen intermittently flashing over resumed gameplay. Inspection
of both native and interpolated source images did not reveal an all-black frame;
these captures precede final presentation. Capture 004 reports no current
interpolation pair, so its older `-interp.ppm` file must not be treated as current.

The user then disabled the system Adaptive Sync setting and tested the same build
at high FPS. They reported that the black flashes disappeared, and confirmed
that the pause-screen flashes also did not recur after reproducing the previous
test: pausing, leaving the game paused for a while, then resuming. No renderer
changes were made between these tests. This strongly implicates the system's
Adaptive Sync/display path for these two remaining symptoms. Treat disabling
Adaptive Sync as the user-confirmed working configuration; do not attribute
this resolution to the preceding geometry patches. Those patches addressed
separately reproduced geometry correspondence/repair defects. An underlying
driver, compositor, monitor, or frame-pacing mechanism has not been isolated.

The project drive remounted after reboot at `/run/media/bubba/1TB SSD1`, replacing
the old `1TB SSD` mount path. The ignored `.dreamcomp/local.json` saved disc path
was updated accordingly. Current launch command:

```sh
cd "/run/media/bubba/1TB SSD1/Projects/Sarge's Heroes Recomp/work" && python3 src/dreamcomp/tools/dc.py run ports/army-men-sarges-heroes-recomp --window
```

The existing CMake build cache still contains the old absolute mount paths; a
future rebuild needs to address that relocation. The existing executable runs
from the new location, and its code was unchanged during the Adaptive Sync test.


Build relocation follow-up (logo-skipping work): the cached mount prefix has
now been corrected and CMake regenerated successfully at `1TB SSD1`, reusing
its downloaded dependencies. The game has been rebuilt there; the earlier
build-cache relocation warning above is resolved.
