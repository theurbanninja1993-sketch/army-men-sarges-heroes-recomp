# Native mouse camera, 2026-10-21

Implemented in `src/mouse_camera.cpp`, with an entry hook on `0x0C0572A8`.
This replaces mouse steering in this port with relative yaw/pitch changes. WASD
continues through the existing Maple stick bindings. F9 releases/captures the mouse.
Other dreamcomp titles retain the existing virtual stick unless they subscribe to
the new raw mouse interface.

## Settings (updated 2026-10-22)

Open **Escape → Controls** for **Mouse camera** (Horizontal only / Full camera)
and separate **Mouse camera sensitivity** and **Mouse aim sensitivity** sliders. Click **Apply** to save and apply changes to
the running game. Full camera remains the default. Horizontal only ignores vertical
mouse motion and restores the original pitch when selected.

Sensitivity is shown as a percentage: 100% is the original 0.12 degrees per relative
unit. The slider covers 2–200%, with 2% steps using Left/Right. The same settings
also work through `--set`; add `--save-settings` to persist command-line changes.

| Setting | Default | Meaning |
| --- | --- | --- |
| `mouse_look` | `true` | Enable native camera motion |
| `mouse_sensitivity` | `0.12` | Normal camera degrees per relative mouse unit; accepted range 0.001–5 |
| `mouse_aim_sensitivity` | Existing normal sensitivity | RMB aim camera degrees per relative mouse unit; accepted range 0.001–5 |
| `mouse_invert_y` | `false` | Invert vertical mouse motion |
| `mouse_vertical` | `true` | Full camera (`true`) or horizontal mouse movement only (`false`) |

Example: `--set mouse_sensitivity=0.18 --set mouse_invert_y=true --save-settings`.
Mouse input is suppressed during settings/rebinding, loss of focus, and F9 release.
Cinematic cameras do not receive the gameplay hook's angle changes.

A/D now engage the native strafe trigger immediately alongside their stick direction.
They move left/right without rotating the actor or camera; W/S retain their existing
movement. This applies to the keyboard's rebound sideways stick keys too. Controller
stick input retains its original behavior. The mouse is released while the settings
menu is open so its options can be clicked, then captured again when the menu closes.

Live changes are delivered through `PortInfo::on_settings_changed` on the guest
thread after Apply. Camera code caches these values instead of reading the settings
map while the menu worker may be writing a draft.

## Measured game structures

Addresses below are for the supported T-9715N V1.001 binary. Use function arguments
and the camera's pointers at runtime, rather than the sample object addresses.

| Function | Role |
| --- | --- |
| `0x0C0572A8` | Camera update: follow callbacks, orbit/collision, matrices, submission |
| `0x0C057150` | Submits projection and view matrices from a camera object |
| `0x0C01E608` | Copies view matrix to renderer state at `0x8C145888` |
| `0x0C057964` | Third-person follow callback (mode 1) |
| `0x0C057DA8` | First-person targeting callback (mode 4) |
| `0x0C0418DC` | Regenerates actor float heading from its 16-bit heading |
| `0x0C0563F0` | Processes native input, including targeting pitch |

Camera `r4` at update entry:

| Offset | Field |
| --- | --- |
| `+0x00` | Follow target position |
| `+0x0C` | Pitch in degrees (signed/wrapped depending on native path) |
| `+0x10` | Yaw in degrees |
| `+0x18` | Computed eye position |
| `+0x64` | Mode (1 follow, 4 targeting, 6 observed in cinematics) |
| `+0x94` | Position source pointer |
| `+0xA4` | Actor angles pointer (actor base +0x18) |
| `+0xA8` | Follow pitch offset (10 degrees initially, reset by native mode changes) |
| `+0xDC` | Camera callback |
| `+0x214` | Projection for the Dreamcast renderer |
| `+0x254` | View matrix passed to `0x0C01E608` |

In the first mission, the gameplay camera was `0x8C1A1010`; the earlier cinematic
camera was `0x8C1A06E0`. The player actor was `0x8C411804`: position at +0x0C,
float heading at +0x1C, and **authoritative u16 heading at +0x78** (65536 = 360°).
The hook changes the u16 and float headings together, plus camera yaw, so native
actor updates do not undo the mouse turn on the next frame.

Third-person pitch uses the camera's +0xA8 orbit offset. Targeting pitch uses its
+0x0C field. Changing actor pitch instead makes the follow orbit and follow
smoothing disagree; merely changing renderer matrix `0x8C145888` is overwritten
by subsequent submissions. The previous vblank matrix experiments were therefore
not sufficient evidence that this was a player-model matrix.

Gameplay camera calls can occur every other vblank. Relative input must accumulate
until the camera consumes it; clearing it every vblank loses alternating samples.
Unconsumed samples expire after two vblanks, and inactive capture clears them.

## Verification and limitations

Controlled first-mission runs, fixed RTC seed 1000000, briefing continued with
`a@7800`, motion at frame 8400, RAM checked at frame 8450:

- 250 horizontal / 125 vertical units at sensitivity 0.12: heading advances by
  29.9982 degrees and remains there after motion stops. Third-person pitch changes
  from 10 to 25 degrees; targeting pitch changes from 0 to 15 degrees.
- Player X/Z are unchanged by mouse-only input. Native camera collision and actor
  grounding may change eye/player Y slightly.
- Disabled mouse look produces a RAM dump identical to the baseline.
- Motion during the title/menu produces a RAM dump identical to the baseline.
- Sensitivity 0.24 with vertical inversion gives +60° yaw / −30° pitch in targeting;
  a large sample wraps yaw and clamps pitch at +75°.
- Horizontal-only mode preserves original third-person/targeting pitch while yaw
  still advances 30°. Native strafe input moves left/right with unchanged heading.
- An SDL keyboard-state test held D through the real window polling/binding path:
  player X/Z changed, while actor and camera yaw both remained 6.3501°.
- A scripted Escape → Controls session selected Horizontal only, adjusted the
  slider to 108%, and clicked Apply. The menu displayed Applied, saved
  `mouse_vertical=false` / `mouse_sensitivity=0.129600`, and delivered the live
  change to the camera on the guest thread. Screenshot: `shots/mouse-options.png`.
- Captured display lists rendered with Vulkan show the room turning right and
  down in targeting mode; HUD and crosshair remain in place.
- Runs report zero untranslated call targets and only the previously documented
  unmapped write at `0x8BFFF1C0`.

The user confirmed the initial native mouse control works well. Combined controls
and other campaign levels still benefit from play testing. Native view/mode transitions retain their original
behavior and can reset pitch. First-person shooting accuracy has not been checked
against a target; this implementation preserves the native heading/camera inputs.

Diagnostics are opt-in: `ARMY_TEST_MOUSE=FRAME:DX:DY` injects one sample even in a
headless run; `ARMY_TRACE_CAMERA=1` logs consumed samples. They are inert normally.

Reproduce the twelve headless scenarios with:

```sh
python3 tools/verify_mouse_camera.py --disc "../../disc/gdi/army-men-sarges-heroes.gdi"
```

The script uses empty settings, checks persisted angles 50 frames after motion,
compares whole RAM for disabled/menu cases, and checks the runtime fault report.
The strafe scenarios use the optional sixth `DREAM_EXP_PAD` field for the right trigger.

## Engine dependency

The approved edits are already applied in `work/src/dreamcomp`. The isolated patch
is `patches/dreamcomp-native-mouse.patch`. It applies on top of the engine's earlier
local input/renderer changes; it is not a replacement for that full patch set.
`Extension::wants_mouse_motion()` selects native input, and
`on_mouse_motion(dx, dy, active)` runs on the guest thread during input polling.
Window capture still uses F9. Inactive input clears pending samples; the boot loop
also blocks motion while settings or the binding screen are open.

The follow-up patch `patches/dreamcomp-mouse-options-strafe.patch` adds the Controls
widgets and Apply support, `PortInfo::mouse_camera` / `on_settings_changed`, keyboard
strafe selection, settings-menu mouse release, and the optional debug right trigger.
Apply it after `dreamcomp-native-mouse.patch` on the earlier local engine checkout.

## Separate aiming sensitivity (2026-10-22)

`src/mouse_camera.cpp` selects `mouse_aim_sensitivity` for native targeting camera
mode 4 and `mouse_sensitivity` for follow mode 1. Both yaw and pitch use the
selected speed. Controller stick rates remain separate. The first settings refresh
initializes an absent aim key from the existing normal sensitivity, preserving
current feel; later changes are independent. Apply delivers both values through
the existing guest-thread settings callback.

`patches/dreamcomp-mouse-aim-sensitivity.patch` updates the Controls UI after the
prior engine patches. Both sliders use the existing 2–200% range, 2% keyboard
steps, and 100%=0.12 degrees/unit scale. Inversion and horizontal-only mode still
apply to both cameras.

The playable executable was rebuilt. `tools/verify_mouse_camera.py` passes 14
native first-mission scenarios, including separate-normal (normal .12 / aim .06
produces +30 yaw / +15 pitch in follow) and separate-aim (normal .24 / aim .06
produces +15 yaw / +7.5 pitch in aim), plus the existing settings, clamp, input
isolation and strafe checks. Test settings and VMU paths are now unique per case
under the output directory, so default migration cannot couple concurrent runs.
Logs and results: /tmp/army-separate-sensitivity-check/.

A live Escape overlay run started with only mouse_sensitivity=.06 and displayed
both sliders at 50%. Clicking the aim slider midpoint and Apply saved
mouse_aim_sensitivity=.12 while mouse_sensitivity stayed .06. The in-game Apply
callback ran. Logs/settings: /tmp/army-aim-sensitivity-ui.{log,ini}; screenshot:
shots/mouse-separate-sensitivity.png. User settings and saves were not used.

## RMB hold dropping aim in Shrap (2026-10-22)

The user reported flashing in/out of aim, supplied the two newest captures, and
identified Shrap rather than Snow; they also reported a correlation with sloping
ground and standing position. Capture-000/001 were inspected and show normal
third-person frames in Shrap. Their earlier paired interpolation companions are
older and were not treated as current evidence.

Cause reproduced: keyboard aim and Modern controller profiles were enabled only
while the last recognized gameplay camera update was at most two vblanks old.
Shrap can miss that deadline. A continuous physical RMB sample then alternated
between the mapped full-strength left trigger and stock digital trigger ramping,
which starts from zero when the mapping drops. The game briefly exits aim despite
RMB never being released. This is an input-profile timing bug, separate from the
new sensitivity values. Terrain-specific cancellation in the base game has not
been established; the user's exact slope position still needs confirmation.

`src/mouse_camera.cpp` now selects gameplay input profiles from mission phase 3
at vblank, while retaining native menu-inhibit gating. Camera callback/mode checks
still limit actual angle edits to supported follow/aim cameras. Input mapping no
longer switches off because of camera timing. The same correction applies to
Modern controller profiles, preventing unexpected changes to stock button roles.
Classic controls remain stock.

### Evidence and checks

- First-mission physical RMB holds were stable at 60 and interpolated high FPS.
- Shrap is selected with native code SRFPNK, then native Start launches the game.
  Temporary diagnostic code holds RMB from ten through 310 vblanks after mission
  phase 3 begins, via a separate SDL input-state shim. Real desktop inputs were
  filtered from this diagnostic so focus and unrelated keystrokes cannot alter it.
- Old build: camera age reaches three at frame 5894; mode 4 starts at 5895 but
  drops to mode 1 at 5897–5898 while RMB remains held, then returns to mode 4.
  Log: /tmp/army-rmb-shrap-entered.log.
- Corrected build, same sequence: mode 4 starts at 5895 and stays continuously
  in aim for all 300 recorded vblanks through 6194, then releases on the test's
  RMB release. Log/RAM: /tmp/army-rmb-shrap-fixed.{log,ram}. Zero untranslated
  calls and only the known unmapped 0x8bfff1c0 write.
- Ten existing native controller-camera scenarios pass, including Classic,
  Modern, native pause and cinematic suppression. Pure input mapping checks pass.
  Results: /tmp/army-rmb-controller-regression/. Controller test settings/cards
  are now unique per case, matching the mouse test's isolation policy.

All forced-RMB, story-advance and per-frame diagnostic source code was removed
before rebuilding the playable executable. The shim is /tmp-only, never loaded
by normal launches. User settings and saves were not used. Confirmation at the
user's exact sloped location is pending.
