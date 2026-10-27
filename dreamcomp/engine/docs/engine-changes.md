# Local engine changes

## 2026-10-21: native relative mouse input for Army Men

`runtime/include/dream/runtime/host_ext.h` adds opt-in
`wants_mouse_motion()` and `on_mouse_motion(dx, dy, active)` extension methods.
`render/include/dream/render/vk/window.h` and `render/src/vk/window.cpp` expose
relative samples during input polling; a subscriber bypasses legacy virtual-stick
steering. F9 still toggles capture. Inactive capture, focus loss, or blocked input
produces inactive samples so ports can clear pending movement.
`runtime/boot/boot_main.cpp` delivers samples on the guest thread and suppresses
motion during settings and controller rebinding. Existing titles without a
subscriber retain their input path.

The Army Men port contains the isolated patch, reproduction script, measured
camera structures and verification in `docs/findings/mouse-camera.md`. The wider
project's `HANDOFF.md` records the earlier local disc, renderer, input, and tool
changes that predate this record.

## 2026-10-21: mouse settings and keyboard strafe

`PortInfo::mouse_camera` exposes Horizontal only / Full camera and a sensitivity
slider under Controls, including that tab's Apply button. `on_settings_changed`
notifies the port on the guest thread after Apply. Settings are cached by the port
rather than polled while the UI worker can edit them. Frontend slider actions use
the same saved mouse_sensitivity value as command-line settings (100% = 0.12).

Extensions can opt into keyboard strafing. Sideways keyboard stick bindings engage
the right trigger at full value immediately, preserving controller input behavior
and avoiding turn frames while a digital trigger would otherwise ramp up.
Window relative mouse capture is released when the settings overlay blocks game
input, then restored on close. DREAM_EXP_PAD accepts an optional sixth right-trigger
field for controlled strafe checks. Existing five-field experiments remain valid.

The port contains `patches/dreamcomp-mouse-options-strafe.patch`, documentation,
12 gameplay checks, and the recorded scripted UI / SDL keyboard verification.

## 2026-10-21 — Army Men capture, widescreen, interpolation

Window capture changes synchronously run on SDL's main thread. Optional
DREAM_TRACE_MOUSE_CAPTURE logs requested/actual state and errors. Verified Escape
release/restoration in a real SDL window.

PortInfo::pillarbox_2d opts into preserving entirely flat movie/menu frames in
widescreen; HUD correction runs only when the frame was not already pillarboxed.
Synthetic HUD tests and Army Men gameplay/movie screenshots passed.

Frame interpolation now reconciles duplicated shared 3D corners after strip
blending. Unmatched copies inherit consistent motion; conflicting matches retain
the native corner. Flat-only sprites/HUD layers remain independent. This closes
reproduced bright ground gaps between interpolated terrain strips without changing
texture sampling. Army Men contains standalone regression tests and a Vulkan TA
capture replay tool, plus isolated patches for all three changes.

### 2026-10-21 — Fix FPS overlay slowdown at high scale

FPS overlay forced full rendered image readback/CPU decoration/reupload and also
disabled interpolation. New separate cached GPU counter layer preserves direct
presentation and intermediate frames. Real Vulkan startup checks: median frame
time at 3x 3.28ms off/3.31ms on; at 8x 4.43ms off/4.42ms on. All four runs
presented 1008 intermediate frames. Counter screenshot visually checked. Rebuilt
game ready; isolated patch `patches/dreamcomp-gpu-fps-counter.patch`.

## 2026-10-21 — Opt-in native controller camera

Host extensions can opt into controller_camera_enabled and receive normalized
player-one right-stick samples through on_controller_camera. Window samples with
the configured deadzone, rescales/squares the remaining range, and blocks camera
input under host menus, binding capture, focus loss or disconnection. In Modern
mode right-stick axes are reserved for the camera; sideways left-stick movement
engages the native strafe trigger immediately. Other players retain their paths.

PortInfo::controller_camera exposes a Classic/Modern row in Controls. Apply also
works for controller-camera-only ports. Army Men opts in and shares its native
mouse camera hook, with independent controller rates/settings. Classic is default.
Nine headless native scenarios, twelve existing mouse checks, four real SDL
virtual-controller runs, and the Escape Apply/save path verified. Isolated patch
is in the Army Men port: patches/dreamcomp-controller-camera.patch.

## 2026-10-21 — Contextual keyboard and controller input profiles

Optional GamepadSample/KeyboardSample callbacks let a port map physical inputs
to logical Dreamcast actions before the two devices are merged. Keyboard/mouse
bindings and Classic gamepad bindings remain separate and unchanged. Mapped
profiles support immediate trigger values; native input mapping still handles
menus/capture/focus as usual. Prior automatic controller strafe is removed; the
Army Men Modern profile requires RB, using native R-trigger for aim quick zoom.

Army Men additionally caches native menu inhibition (0x8c18e770), since native
pause continues camera updates. Profile fallback and camera clearing preserve
pause menu controls. Real SDL controller and keyboard/mouse polling checks pass,
including unmodified Classic and conditional aim behavior. Select/Back already
opens the port overlay; verified open/close and mouse release/restore.
PortInfo::controller_help optionally describes a port's specific mode controls.
Isolated patch in Army Men: patches/dreamcomp-contextual-controller.patch.
