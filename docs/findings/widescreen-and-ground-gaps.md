# Widescreen, Escape capture, and moving ground gaps (2026-10-21)

## Escape mouse capture

SDL3 requires `SDL_SetWindowRelativeMouseMode` on the main thread. Calling it on
Dreamcomp's guest thread left capture active despite the overlay state changing.
`Window::apply_mouse_capture` now synchronously dispatches through
`SDL_RunOnMainThread` when necessary. `DREAM_TRACE_MOUSE_CAPTURE=1` prints actual
capture state. A real window Escape/open/close check verified 1 → 0 → 1, all
operations on the main thread and successful. Patch:
`patches/dreamcomp-mouse-capture-thread.patch`.

## Widescreen

Select Escape → Graphics → Aspect ratio → Expanded → Apply, then use a wide
window or fullscreen. The port supports up to 16:9 with additional horizontal
view and unchanged vertical FOV. Original aspect remains available.

`src/widescreen.cpp` updates camera +0xe8 for visibility/culling, and hooks
`0x0c01e5f4` only when called from camera submission (`PR=0x0c05720a`). The
freshly rebuilt Dreamcast projection's horizontal focal scale is multiplied by
(4/3)/aspect before copying to renderer globals. Vertical focal scale is unchanged.
Camera update applies aspect even when mouse motion is inactive.

Port HUD correction keeps the health/ammo/radar proportions and anchoring.
The opt-in framework `pillarbox_2d` also centers entirely flat frames (movies,
menus) with black sidebars before HUD correction. Patch:
`patches/dreamcomp-wide-2d.patch`.

Verification: deterministic first-mission 4:3 RAM dump exactly matched the
pre-widescreen baseline. 16:9 camera aspect 1.777778, conventional projection X
1.357995 versus 1.810660 at 4:3, Dreamcast focal X 180 versus 240; Y stays 240.
Screenshots in ignored `shots/widescreen/` verify expanded gameplay and undistorted
movie/loading screens. Framework HUD tests passed, including new flat-frame cases.

## Bright ground gaps during movement

Reproduced the user's description using native TA captures while walking forward
in the first mission. Native frames were intact. Engine interpolation matched
strips by render state and blended each independently; missing/skipped or wrongly
paired strips separated duplicated shared corners. Replaying an intermediate
frame exposed a sky-colored diagonal gap across the grass. Before correction,
40 shared corners differed, up to 32.642 guest pixels.

`patches/dreamcomp-interpolation-seams.patch` keeps copies of a shared 3D corner
coincident after blending. Matched motion propagates to unmatched copies;
conflicting proposals conservatively retain the newer native corner. Independent
flat sprites/HUD layers remain independent. Texture sampling and native frames
are unchanged. This preserves interpolation rather than disabling it.

All 59 intermediate frames in the captured movement sequence now report zero
split shared terrain vertices (worst separation 0.000 pixels). The reproduced
bright gap is absent in the fixed replay.

Regression checks cover newly appearing geometry, conflicting matches, flat HUD
independence, and camera cuts. Build/run:

```
cmake --build build --target armymensh_interpolation_tests -j4
./build/tools/armymensh_interpolation_tests
```

`armymensh_render_capture` is an optional headless Vulkan capture replayer:

```
cmake --build build --target armymensh_render_capture -j4
./build/tools/armymensh_render_capture newer.ta capture.vram out.png --previous older.ta
```

Keep matching `.vram.regs` beside the VRAM file. Replay uses a single VRAM snapshot;
animated textures may consequently be stale across a sequence. `--wrap`, `--point`,
and `--snap` are diagnostic comparisons, not game defaults. The shared terrain
corner diagnostic excludes flat sprites. Captures contain disc-derived data and
must stay out of git; `capture-*` data patterns are ignored.

Launcher compatibility: optional diagnostic executables live in `build/tools/`,
leaving only the game in `build/game/` for `dc.py` executable discovery.

## Follow-up: story text and cutscene borders

User captures `capture-000` and `capture-001` (2026-10-21 11:56) exposed two HUD
classification errors. An isolated final lowercase “a” in the story paragraph
anchored to the right third independently of its line. The two native side border
strips grouped together because their tall extents passed the text-line gap test,
then moved toward the screen center.

`game/hud_overrides.ini` centers the resident-font paragraph (TCW 0x04060010,
y=180..320, excluding gameplay overlays) and leaves untextured frame strips at
x=0/640 in native screen coordinates. Capture replay before/after confirmed the
letter rejoins its line and both inset gray bars disappear. No renderer or native
guest memory change is required. The game logs three port HUD overrides at startup.

The replay tool now accepts `--inspect` for flat primitive diagnostics, and
`--wide [--hud game/hud_overrides.ini]` to reproduce the live widescreen HUD pass.
Its wide PNG output is 1280x720. Example:

```
./build/tools/armymensh_render_capture capture-001.ta capture-001.vram /tmp/cutscene.png --wide --hud game/hud_overrides.ini
```

## FPS overlay at high internal resolutions

The FPS flag used to disable direct GPU presentation and interpolation, forcing a
full offscreen readback, CPU decoration, and upload every displayed frame. Pixel
traffic grows quadratically with internal scale. `dreamcomp-gpu-fps-counter.patch`
uses a separate transparent GPU overlay instead, with a 640-wide logical canvas
updated only when the numbers or window aspect change. It is also drawn on the
intermediate frame; normal game captures remain clean. The FPS counter is hidden
under the extension settings overlay.

Real Vulkan 1200-frame deterministic unthrottled comparisons at 16:9:

| Scale | Counter off median | Counter on median |
|---|---:|---:|
| 3x | 3.28 ms | 3.31 ms |
| 8x | 4.43 ms | 4.42 ms |

All four runs presented 1008 interpolated frames. The 3x counter screenshot was
visually checked (`shots/fps-counter/gpu-counter.png`). These are startup sequence
measurements, not a full-level performance benchmark. User game speed still uses
the normal pacing settings; the benchmark explicitly disabled pacing.

## Follow-up: enemy target marker bouncing in widescreen

The enemy target ring is a world billboard with texture word `0x9007b3d0`,
submitted as two triangles. The HUD classifier accepts flat polygons at a shared
depth. Small floating point differences between the ring's vertex depths make it
alternate between being classified as HUD and being left alone. When classified,
HUD correction squeezes its X coordinates about a screen anchor, moving it away
from the soldier. The game has already projected the marker correctly.

The user's 26-capture batch (guest frames 3373–3799) reproduces this without
running the game: frame 13's marker center remains at X=349.530, but frame 14 is
moved from X=349.350 to 342.012. Frames 18–19 retain their native center; frame 20
is moved from X=348.256 to 341.192. This explains the alternating body-relative
offset. The earlier camera/target-selection theories did not identify the cause.

`game/hud_overrides.ini` excludes this marker texture from HUD correction with
`anchor=stretch`, across all screen positions and primitive sizes (`full=1`).
Here "stretch" means preserve the incoming projected vertices. This also avoids
applying a second horizontal squeeze to the marker's already projected width.

Verification: of the 26 captured frames, 25 contain the marker. Before the
override, the HUD pass displaced marker vertices in 16 frames at 16:9, with a
maximum corner displacement of 68.092 logical pixels. After the override, all
450 marker vertices checked across 4:3, 16:10, and 16:9 remain byte-identical to
their native values; other vertices are identical to the previous HUD output.
GPU replay of frame 20 before/after confirms the ring returns to the soldier's
chest. The user confirmed the fix stops the bouncing during actual widescreen
gameplay. Texture residency in other missions remains to be checked. This
correction does not change camera projection,
visibility, target selection, or actor animation.

The three user RAM snapshots `/tmp/army-target.{3618,3649,3682}.bin` were received
successfully; the temporary timed RAM-dump instrumentation was removed once the
HUD cause was reproduced. Captures and RAM dumps must remain outside git.


## Snow terrain follow-up (2026-10-22)

The user's new capture-000 (06:39, guest frame 110443) shows a long black
crack only in the interpolated image. Native geometry is intact. The previous
shared-corner repair required bit-identical projected XYZ coordinates and its
exact-match diagnostic therefore incorrectly reported zero split vertices.

One reproduced boundary is between opaque strips starting at vertices 1675
and 1795, TCW `0x8809216c`. Their corner copies 1677 and 1798 differ only in Y:
`164.876846` versus `164.876862` (one float ULP), with identical X `582.046326`
and reciprocal depth `1.37174594`. Before the fix, one corner blends to
`(581.070984,164.459946)` while the other stays native, opening the crack.

Shared engine patch: `patches/dreamcomp-interpolation-snow-corners.patch`, applied
after the previous interpolation patches. Shared world corners now join within
0.001 guest screen pixels and a tight reciprocal-depth tolerance (relative
1e-6, absolute floor 1e-7). Neighbouring spatial cells are searched so corners
across a quantization boundary still join. Coincident flat UI layers remain
independent; distinct depth layers do not join. Shared-corner conflict handling
and local folded-triangle rollback use the same groups; rollback restores one
canonical native position for every copy of the corner.

Verification:

- The added one-ULP/quantization-boundary regression fails against the previous
  implementation and passes with the fix. Separate-depth and existing flat UI,
  folded-face, correspondence, projection and camera-cut checks pass.
- GPU replay of the exact captured TA pair now shows continuous snow ground.
  The reproduced corner's X/Y gaps are both exactly zero.
- Existing portal transparency regression passes. The playable game and capture
  replayer were rebuilt. Gameplay confirmation is pending a restart.
- A 100-iteration CPU benchmark on the captured 9,059-vertex scene measures
  about 3.3 ms per full blend versus 2.5 ms before; spatial lookup uses a reserved
  hash table to limit the added cost.
- Replay images: `/tmp/army-snow-replay.png` before and
  `/tmp/army-snow-fixed.png` after. Build log: `/tmp/army-snow-final-build.log`.

## Snow opening cutscene audio (2026-10-22)

The user replayed this intro in Flycast and confirmed it is silent there too.
The initial investigation left audio unchanged. The user subsequently requested
a helicopter sound enhancement; see [snow-intro-audio.md](snow-intro-audio.md).

The supplied disc image independently explains the silence: the original game
requests stream ID 45 (`Cin8_1.str`) for Snow's opening cinematic, but that file
is absent from its ISO9660 filesystem. `CIN8_2.STR` is present (411648 bytes).
Native entry `0x0c025ee4` selects the cutscene audio by the level at
`0x8c18ed20` (level 8 dispatches to `0x8c0263ac`); opening cinematic index 0
reaches `0x0c0263ba`, which calls `0x0c055728` with stream ID 45.
The stream table at `0x8c0d6540`, entry size 12, points that ID to filename
`0x8c0d6aeb`. These are facts about this disc image; they do not establish
whether another retail disc revision includes that soundtrack.
