# Classic and Modern controller modes (2026-10-21)

Escape → Controls → Controller mode → Classic / Modern → Apply. The setting saves
as `controller_mode=classic|modern`; Classic is the default. Keyboard/mouse camera options remain separate. Classic bindings are preserved;
Modern uses the contextual gameplay profile described below.

- Classic preserves the original left-stick turning and movement. The right stick
  does not drive the native camera.
- Modern uses the left stick for forward/back movement (hold right bumper for
  sideways strafing) and the right stick for horizontal
  turning and vertical look. Camera yaw and the actor's authoritative heading stay
  together, so firing follows the camera, as with native mouse look.
- Right-stick input uses the controller bindings' configured deadzone, rescales its
  remaining range and squares it for finer aiming near center. Maximum rates are
  180 degrees/second horizontally and 90 vertically. Rates use guest 59.94 Hz and
  accumulate across the native 30 Hz camera updates.
- Controller look is independent of mouse enable/capture, mouse sensitivity,
  mouse horizontal/full mode and mouse inversion. Cinematic cameras are excluded.
  Existing camera pitch limits apply. Menus, binding capture, focus loss and pad
  disconnection release right-stick input. Applying a mode change clears pending
  stick motion.

## Implementation

Port `src/mouse_camera.cpp` reuses the native follow/targeting camera hook for the
right stick, combining its degree deltas with mouse deltas after mouse sensitivity
is applied. Mode settings are cached on the guest thread through the existing
settings callback. `src/armymensh.cpp` opts into the Controls menu capability.

`patches/dreamcomp-controller-camera.patch` adds an opt-in host extension input
API, SDL player-one right-stick sampling, and the Classic/Modern menu row. Modern
reserves right-stick axes from any Dreamcast control bindings. A contextual input mapper now handles Modern gameplay actions; the earlier
automatic left-stick strafe behavior has been replaced by the bumper modifier. Other players keep their existing input path.

## Verification

Deterministic native first-mission scenarios cover Classic no-op, both look
directions, targeting mode, mouse settings independence, pitch clamping, no cinematic motion leaking into gameplay, and native pause suppression:

```
python3 tools/verify_controller_camera.py --disc /path/to/army-men-sarges-heroes.gdi
```

Classic right-stick samples leave the complete RAM dump identical to the baseline.
The previous twelve native mouse/keyboard regression scenarios also pass.

A real SDL virtual controller check compared both modes through Window::poll:
Classic right stick leaves yaw 6.3501/pitch 10; Modern right stick turns actor and
camera together and changes pitch without moving the actor. Classic left stick
turns; Modern left stick moves X/Z (991.806/-424.709 to 1044.189/-410.871) while
actor/camera yaw remains 6.3501. All runs have zero untranslated calls and only
the known `0x8bfff1c0 w4 x1` write fault.

The actual Escape menu was exercised from a Classic start: selected Modern,
clicked Apply, verified `controller_mode=modern` saved, and closed the menu. A
held SDL virtual right stick then drove the native camera without restarting.
Screenshot: `shots/controller-options.png`. Test settings were isolated in /tmp;
the user's saved controller mode remains unchanged.

## Modern shooter bindings and aiming modifiers

| Action | Modern controller | Keyboard/mouse |
|---|---|---|
| Fire | Right trigger | Existing left mouse binding |
| Change weapon | X | Existing binding |
| Prone | B outside aim | Existing binding |
| Jump | A outside aim | Existing binding |
| Aim | Left trigger | Right mouse button |
| Zoom in/out while aiming | D-pad up/down | W/S while RMB held |
| Quick zoom while aiming | Right bumper | Shift while RMB held |
| Strafe | Hold right bumper outside aim, then left stick sideways | Existing A/D outside aim |
| Port settings menu | Select/Back, either mode | Escape |

Controller left-stick horizontal input is ignored without RB and while aiming.
RB sends the original native R-trigger action: strafe outside aim, quick zoom
inside aim. RT sends native X (fire), X sends native B (weapon), and B sends
native Y (prone). During aim, D-pad up/down send native Y/A and stop sending
their original directional actions; face A/B do not accidentally change zoom.
Forward/back controller movement remains on the left stick.

For MKB, W/S send native Y/A during RMB aim and stop sending forward/back
movement. Shift supplies the native quick-zoom trigger immediately. Automatic
keyboard strafe triggering is disabled during this aiming map, so A/D cannot
accidentally quick-zoom. Releasing RMB restores normal keyboard bindings. This
works independently of Classic/Modern controller selection.

The port selects contextual profiles only while its native follow/target camera
is updating and native menu input inhibition is clear. The camera keeps updating
under the native pause menu, so camera activity alone is insufficient. Byte
`0x8c18e770` is the game's menu input-inhibit countdown (checked/decremented at
`0x0c0364ba`, setting the input-block flag `0x8c1a3bc0`). It is 0 in gameplay and
1 in the captured pause screen. While inhibited, contextual mappings fall back
to stock and pending mouse/right-stick look is cleared. Stale/missing gameplay
cameras also fall back to ordinary menu/cinematic bindings. The engine samples and maps the keyboard and controller
separately before merging them, preserving simultaneous keyboard/controller use.
No Classic binding file is rewritten. Extra shoulder/select buttons are host
controls; Select/Back was already wired to the port overlay and has been verified
opening/closing it, with actual mouse capture 1 → 0 → 1.

Regression checks for the action table and real SDL input paths:

```
cmake --build build --target armymensh_controls_tests -j4
./build/tools/armymensh_controls_tests
./build/tools/armymensh_controls_tests --window
```

On Linux, the keyboard polling test uses an explicit, test-only shim:

```
cc -shared -fPIC tools/test_keyboard_input_shim.c -o /tmp/army-input-shim.so $(pkg-config --cflags --libs sdl3) -ldl
LD_PRELOAD=/tmp/army-input-shim.so ARMY_INPUT_SHIM=1 ./build/tools/armymensh_controls_tests --window
```

All pass, including Classic RT unchanged, game-menu B unchanged, RB-gated
strafing, LT-only D-pad zoom, RMB+W/S without movement, immediate RMB+Shift,
no accidental aim quick zoom from A/D, and normal W after RMB release.
The nine native camera scenarios also pass after these changes. Engine patch:
`patches/dreamcomp-contextual-controller.patch`, after `dreamcomp-controller-camera.patch`.

The native pause look check retains baseline pitch/yaw/actor heading exactly.
Select/Back was verified with real SDL pad events to open and close port settings;
no Dreamcast Select input is introduced.

## Camera cadence and held inputs (2026-10-22)

Keyboard aim and Modern controller mapping now use native mission phase 3 plus
menu-inhibit gating instead of requiring a gameplay camera update within two
vblanks. Shrap exposed the old freshness check dropping the held RMB trigger
back into stock trigger ramping and briefly exiting aim. Camera edits retain
their own callback/mode guards. Ten native controller-camera scenarios and pure
input mapping checks pass. Reproduction/evidence: the RMB hold section of
`mouse-camera.md`. `verify_controller_camera.py` now isolates settings and VMU
files per scenario to prevent settings migration from coupling concurrent tests.
