# PAL bring-up (2026-11-05)

Experimental development targets only. The v0.1.7 USA release still rejects PAL images.
`armymensh_pal_native` preserves stock behavior. `armymensh_pal_controls` adds the settings
overlay, mouse/modern controller camera, widescreen and PAL native-menu input. Both share the
same translated guest objects. No CDI helicopter replacement is applied. Do not advertise
PAL release support yet: launcher integration and broader gameplay testing remain.

## Latest validation

User play test: completed the entire first level with no gameplay problems and reported smooth
gameplay at 8x resolution. Movies/cutscenes had slowdown and choppy audio. Ground seams,
bouncing target marker and stretched sniper overlay remain reported issues. These observations
supersede the earlier pending first-level manual-test status; other levels remain unverified.


- Stock PAL first mission: 8500 frames, exit 0, **zero untranslated calls**, down from 335293.
  `closed-8500.txt` records the run; its frame8400 display list and rendered image are byte-for-byte
  identical to the earlier interpreter-assisted native baseline.
- Enhanced PAL: six mouse scenarios pass (base, turn, disabled, aim base/turn, horizontal-only),
  plus four controller scenarios (base, unchanged Classic, Modern, paused). All have zero
  untranslated calls. Evidence: `camera-check/` and `controller-check/` under the local evidence
  directory below. Test tools accept `--profile pal-en60 --exe build-release/game/armymensh_pal_controls`.
- Widescreen: 8500 frames, exit 0, zero untranslated calls, inspected `wide-fixed-8400.png`.
  Camera aspect is 16:9, source projection scale stays 240, submitted projection scale is 180.
  Synthetic tests cover camera clones, live aspect switching, split viewports and matrix restoration.
- Snow second-level route: 10000 frames, exit 0, zero untranslated calls. RAM confirms
  gameplay phase 3, level 8 and follow camera mode 1. `snow-9900.png` was rendered and visually
  inspected; health, ammo and map remain correctly placed in widescreen. Evidence:
  `snow-10000.{log,txt,ram}`, `snow-9900.{ta,vram,png}`. This run used `-O0`.
- Optimized (`-O1`) Snow repeat: 10000 frames, exit 0, zero untranslated calls; phase 3, level 8,
  follow camera. Frame9900 TA and all 8 MiB VRAM are byte-identical to the `-O0` run. Headless
  host time is 95.33 s versus 150.80 s for the same 166.827 guest seconds; this does not measure
  windowed/high-refresh FPS. Evidence: `optimized-snow-10000.{log,txt,ram}`,
  `optimized-snow-9900.{ta,vram}`. Both PAL executables are relinked with optimized objects.
- `armymensh_disc_revision_tests` passes, including overlay tail-hook cleanup and native-entry
  resumes. The published USA allowlist remains unchanged.
- Windowed SDL test: 4500 frames, exit 0, zero untranslated calls, 4466 frames presented and
  zero failed texture decodes. Clicks create the scratch save, open Input Code, type `BLZZRX`,
  Backspace, `D`, Enter; the native validator displays **Snow Mission**. Evidence:
  `menu-sdl-fixed-4500.{log,ram}` and `menu-sdl-fixed-4400.png`.
- `armymensh_pal_menu_tests` passes against actual PAL main/password/pause/no-VM snapshots:
  widescreen hitboxes and side bars, text editing/limits, pause rows, pointer release and gameplay
  input isolation. PAL's no-VM dialog (state 2) puts A/B at logical y157; state 5 keeps the
  original y97 hitboxes. The USA profile keeps its existing state-5 handling.

**First milestone achieved:** the isolated native loader+English60 build completes an
8500-frame run and reaches the first mission. The frame8400 capture was rendered and visually
inspected: Sarge is standing in the starting barracks, with health, rifle/ammo and map visible.
That initial milestone used development-interpreter fallback; the newer `closed-8500` run
eliminates it on the same boot/first-mission route. Other levels still require validation.

## Established boot sequence

The stock top-level PAL `1ST_READ.BIN` runs in the existing SH-4 development interpreter.
At frame 500 its rendered screen shows five languages and 50/60 Hz selection. English and
50 Hz are the defaults. `down@550,a@600,a@750,a@900,a@1050` selects English 60 Hz and gets
through the confirmation. RAM at frame 1250 matches `KLINK/KLINKENGLISH60HZ.BIN` at the main
load address; the default selection instead loads `KLINK/KLINKENGLISH.BIN` there.

The loader performs the secondary load itself through the existing GD-ROM HLE. No custom host
asset-directory rewrite or guessed command-line initialization is required when preserving that
loader. A full 4500-frame interpreter run reaches the game's memory-card prompt. Without
`--vmu`, it correctly asks for a VM and offers B to continue without saving. The rendered title
screen was also inspected. Campaign gameplay has not yet been established by that run.
With a formatted scratch VMU, a 5500-frame stock run reaches the main menu normally; its
screen asks for Start to launch the selected campaign, so repeated A presses do not start it.

Local evidence (not source or release contents): `/tmp/army-disc-versions/pal/`:

- `loader-500.png`: original language/video screen.
- `loader60-1250.ram`, `.txt`: secondary executable identification, normal frame-limit exit.
- `stock60-3000.txt`, `stock60-2900.png`: title screen, normal frame-limit exit.
- `stock60-4500.txt`, `stock60-4400.png`: memory-card prompt, normal frame-limit exit.
- `stock60-5500.txt`, `stock60-5400.png`: main menu with scratch VMU, normal frame-limit exit.
- `native-fixed-8500.txt`, `.ram`, `native-fixed-8400.png`: native first mission, normal
  frame-limit exit (0), 141.804 guest seconds / 143.94 host seconds in this headless debug build.
- `native-fixed-suggest.toml`: observed missing targets and relocation candidates; the report
  compares against the initial loader, so assign each suggestion to its actual executable before
  copying it into a profile.
- All extraction inputs, raw captures and generated translations stay ignored or in `/tmp`.

## Developer targets

Configure with `-DARMYMENSH_PAL_PROBE=ON -DDREAM_DEV_INTERPRETER=ON` using the existing build
environment. Default builds and the released USA target do not include these targets.
Guest optimization defaults to `-O1` (`ARMYMENSH_PAL_OPTIMIZE=ON`); set it OFF for faster
unoptimized iteration. Initial validation runs below used `-O0`.

1. Build `armymensh_pal_extract` and `armymensh_pal_probe`.
2. Run `build/game/armymensh_pal_extract PAL_DISC game/extracted/pal`. It verifies the loader,
   English60 and transfer-container hashes before writing anything, using runtime GDI/CUE/CHD readers.
   USA images are rejected before creating an output directory.
3. Reconfigure, then build `armymensh_pal_native` and/or `armymensh_pal_controls`. This translates
   the original loader, English60, transfer code and observed low-RAM interrupt copies into
   shared objects under `build/game/pal-gen/`.

The interpreter diagnostic run is:

```sh
build/game/armymensh_pal_probe --interpret \
  --config game/armymensh_pal_probe.toml --disc "PAL_DISC" \
  --no-audio --max-frames 1250 \
  --press down@550,a@600,a@750,a@900,a@1050
```

The native experiment uses `armymensh_pal_native` with the same bootstrap config and disc,
without `--interpret`. Pass a separate scratch `--vmu /tmp/army-pal-vmu.bin` for campaign runs.
`game/armymensh_pal_en60.toml` is translation input, not a direct disc-boot profile.
`GINXFER.BIN` has an 8192-byte container header. Its 244-byte payload, extracted as
`GINXFER.CODE.BIN`, exactly matches RAM at `0x8CFD2000`. The extractor verifies the full container
SHA-1 `08e56771a4b0ff8dd6cf2896902e94fed7402fd0`; the translated payload SHA-1 is
`87ed534bde9293e3ca0193e8f85adf2b4b0eb0ba`. No raw container bytes are treated as instructions.

The stock target supplies no-op hooks. The enhanced target uses `ARMY_PAL_EN60` and
`src/game_layout.h`; USA constants remain the default. Address mapping uses unique normalized
instruction windows and the actual menu/gameplay RAM snapshots, rather than a common offset.
The PAL follow and aim callbacks uniquely match the USA instruction prefixes, and the camera's
mode, angle pointer, pitch, aspect and actor heading offsets agree in both versions.

| Field / function | USA | PAL English60 |
|---|---|---|
| Mission phase | `0x8C18ED1C` | `0x8C1A6174` |
| Native input inhibit | `0x8C18E770` | `0x8C1A5BC0` |
| Camera update hook | `0x0C0572A8` | `0x0C05B790` |
| Follow / aim callbacks | `0x0C057964` / `0x0C057DA8` | `0x0C05BE4C` / `0x0C05C290` |
| Projection submit / caller PR | `0x0C01E5F4` / `0x0C05720A` | `0x0C01D0B4` / `0x0C05B6F2` |
| Frontend screen / substate | `0x8C0EB1D0` / `0x8C1A53C0` | `0x8C0CE8D0` / `0x8C18F850` |
| Password buffer | `0x8C1A5338` | `0x8C18F7C8` |

PAL creates a camera by copying an already-expanded camera during startup. The aspect helper
now remembers the native origin of written ratios, so the clone stays 16:9 instead of expanding
again to 2.37037. Aspect changes restore tracked native values before applying the new ratio.
Exit-hook functions retain their tail-call frame until the callee completes, so projection-copy
cleanup runs after memcpy rather than being bypassed by overlay tail unwinding.

## Native dispatch considerations

Both binaries occupy `0x8C010000`. Translation uses `--overlay-full --no-fold`: complete function
extents are signed, pools stay live, and native calls dispatch through the runtime so the old
loader cannot bypass overlay checks after the game overwrites it. Short prefixes are insufficient
for different executable builds. Full signatures are intentionally conservative: a mutable byte
in a discovered function extent forces interpretation, which can cost performance.

The generic boot harness now supplies RAM when finding its initial entry, allowing signed
primary units. The translator also preserves register targets for `--no-fold` JSR/BSRF: previously
its direct-call optimization could re-embed a pointer from the input image despite dynamic
literal loads. Regression coverage is in `armymensh_disc_revision_tests`.

The first native experiment reaches 1500 frames normally, including the English60 handoff,
with 103793 interpreter fallback calls concentrated in low-RAM IRQ stubs and late callbacks.
`native-gdb.log` records that run. A longer run overflows the host stack alternating between
discovered fragments `0x0C073A30` and `0x0C073A4E`: their loop crosses native function ranges.
Overlay conditional branches, BRA and known fallthrough tail edges now use the existing
nonlocal-return runner; register tail jumps do likewise. That drops abandoned host frames and
resumes at the actual guest PC. GDB evidence before this fix is `native-gdb-4500.log`.
The runner also resolves exact function entries before containing ranges, allowing tail transfers
to native BIOS entries with no guest-code extent. A synthetic native-entry resume regression
checks this path; without it the first BIOS tail jump tried to interpret the hook's `0xffff` RAM.
The completed 8500-frame native campaign run passes after both fixes. It records 335293 fallback
calls, mostly low-RAM stubs and `0x8C13B854`, plus other undiscovered English60 callbacks. The
stack-overflow fix is therefore validated beyond its original failure point and into gameplay.
Regressions also pass for short-versus-full signatures, live indirect-call targets, split-loop
edges, exact native-entry resumes, USA/CDI acceptance, and PAL rejection by the release profile.
The extraction tool succeeds for PAL GDI and Europe CUE and rejects USA without writing files.
Fresh generation of both primary units is byte-identical to the compiled units; the final
experiment was relinked with the corrected runtime after the BIOS-resume fix.

The first live window test found an unrelated presentation failure at the loader/game handoff.
While the guest display was disabled, `Live::present` kept old GPU-frame dimensions but switched
to an empty CPU pixel buffer. `Presenter::upload` crashed in memcpy. Presentation now changes
its source only when a new displayable frame exists; otherwise the swapchain retains its last
presented image. The subsequent windowed SDL/password test crosses that handoff normally.
GDB evidence: `menu-sdl-gdb.log` (before fix); successful run: `menu-sdl-fixed-4500.log`.

Campaign navigation used for this run (with a separate formatted scratch VMU):

```text
down@550,a@600,a@750,a@900,a@1050,start@2900,down@3100,a@3300,a@3500,start@3700,a@3900,a@7800
```

Initial discovery emitted 3804 loader functions and 5389 English60 functions. The measured
extra callbacks now produce 3805 loader functions, 5411 English60 functions and seven transfer
functions, plus the low-RAM units. Linear sweeping
reports 132 and 266 unlowered instructions respectively; these are discovery diagnostics, not
proof that any failing instruction is executed. The measured first-mission route has no fallback;
new routes still need discovery/testing. The optimized build uses the same modest `-O1` level as
the USA generated code; initial `-O0` results are not release-performance measurements.

## Remaining work

- Validate additional levels and close any newly observed missing targets/overlays.
- Compare native results with stock interpreter captures and exercise multiple levels.
- Manual gameplay validation of the enhanced target, including high-refresh interpolation,
  targeting markers, cutscenes and level changes.
- Select/verify PAL profiles in the launcher, then integrate release packaging and Windows.
- Other languages and 50 Hz need their own measured translations and timing validation.

## Local play test

From the port root, run:

```sh
tools/run_pal.sh "PATH_TO_PAL_GDI_OR_CUE"
```

The helper itself passed a 1250-frame real-window run across the English60 handoff (exit 0,
zero untranslated calls; `helper-1250.{log,txt}`).

Select **English** and **60 Hz** in the original startup menu. The experimental build translates
English60 only. Its settings/save identity is separate from the released USA port. Start with
60 FPS while checking gameplay, then test interpolation separately. The stable release launcher
still rejects PAL; this helper uses the separate experimental backend.

## PAL visual parity and movie performance follow-up

- The shared renderer already contains ground-corner welding, planar-panel and wall-layer fixes;
  the full interpolation regression suite still passes. PAL ground seams need a matching capture
  to determine whether this is interpolation or native rasterization.
- PAL ring TCW is `0x9007d3d0`, versus USA `0x9007b3d0`. The 32x32 texture and 4096 bytes of
  surrounding content match between USA capture-022 and PAL wide-fixed-8400 VRAM. Separate
  profile override files prevent PAL addresses from changing USA HUD behavior.
- Scope texture still needs identification in a PAL aiming capture; USA TCWs are not evidence
  of the PAL allocation.
- Real-window 3000-frame English60 startup/movie route with audio, 60 FPS setting and unthrottled
  execution: 50.054 guest seconds /39.02 host seconds, zero untranslated. Full RAM signatures
  compared in bulk (still checked on every lookup) reduce the same route to35.42 host seconds.
  These averages do not prove individual movie frames meet their deadlines.
- Built-in host sampling is Windows-only. A GDB sample of the actual Linux PAL process found
  exception unwinding in27/35 samples; several are split codec loops around0x0c13ba82..0x0c13bf7c.
  Evidence: `/tmp/army-disc-versions/pal/movie-samples.log`, sample.gdb.
- Added bounded tail handoff at the guest runner boundary. Nested indirect calls still use the
  existing nonlocal exception, exit-hook calls retain cleanup, and nested runners restore their
  predecessor. Synthetic100000-iteration loops, nested caller unwinding, nested runner restoration,
  live RAM signature mutations and existing translator regressions pass. PAL regeneration/build
  completed. Movie/gameplay retests are described below; subjective audio confirmation remains.

### Deferred-tail validation

The final enhanced PAL executable completes the identical windowed/audio3000-frame route in
26.30host seconds (50.054guest seconds), versus39.02original and35.42bulk-signature-only.
Rendered/presented/decoded counts and guest cycles are unchanged; zero untranslated calls and
zero failed texture decodes. Unthrottled playback intentionally drops excess audio, so this is
throughput evidence, not an audible-playback check. Evidence: `tail-movie.{log,txt}`.

First mission8500-frame headless retest passes, exit0, zero untranslated. Frame8400 TA and all
8MiB of VRAM are byte-identical to `wide-fixed-8400` from before this follow-up. Camera aspect
is16:9, source projection240 and submitted180, confirming exit-hook cleanup survives the new
runner path. RAM confirms phase3/level1. Host time42.34s for141.804guest seconds; this is not a
windowed FPS measurement. Evidence: `tail-first-8500.{log,txt,ram}`, `tail-first-8400.{ta,vram}`.

The enhanced PAL executable loads8rules from its separate HUD file; the ninth USA scope rule
is deliberately omitted until its PAL texture allocation is measured. Reticle correction still
needs user gameplay confirmation. Normal paced movie/audio test passes (`paced-movie.{log,txt}`): 3000frames, exit0,
zero untranslated calls, zero failed texture decodes, zero audio drops and zero underruns.
Host52.35s/guest50.054s; frame intervals p50=16.68ms, p95=23.05, p99=28.22, max35.2. This is
not perfect60FPS throughout startup; some guest frames miss their deadline. The audio queue
remained supplied on this measured route. User confirmation of audible movie/cutscene behavior
still remains, especially high-refresh playback and the end-of-level helicopter scene.

## PAL logo skip and scope follow-up

The user confirmed the previous PAL performance/reticle changes look good, then requested the
same developer-FMV skip as NTSC. `startup_logos.cpp` now uses executable-specific layout values.
PAL English60 mappings were checked against normalized instruction windows, literal pools and
live RAM (a common code/data offset does not suffice):

| Item | PAL English60 |
| --- | --- |
| Native movie wrapper | `0x0C073628` |
| Decoder status entry | `0x0C12B00E` |
| Logo-loop status caller PR | `0x0C073724` |
| Movie control state | `0x8C0F0AB0` |
| Midway / Saffire filename | `0x8C0F0AF0` / `0x8C0F0AFF` |
| Newly pressed buttons | `0x8C1FE738` |

The input reader uses raw device state at `0x8C1FE6F8` and logical state at `0x8C1FE730`;
the logo hook reads the logical edge mask at +8, not the raw device/pointer structure.
Only IDs1/2 and their exact filename pointers at the measured caller may return finished status3.
This keeps native decoder/audio cleanup and requires a fresh A/Start press per logo.
PAL regeneration and headless integration tests pass: two fresh presses skip both; a continuous
hold skips only Midway and leaves Saffire decoding; no-skip full RAM is byte-identical to the
pre-hook binary at2000frames. All zero untranslated. Evidence `logo-v2-*`. Real-window/audio2200-frame test also passes with both skips, zero
untranslated and zero failed texture decodes; inspected frame2100 continues to the title screen.
Evidence `logo-window.*`, `logo-window-2100.*`.

User scope capture at guest5898/presented5865 (2026-11-05 21:12) identifies a full-screen flat
quad at TCW`0x10070910`, depth60000. The separate PAL HUD file now uses the existing scope
center-and-black-bars correction for that measured ID. GPU replay rendered `scope-fixed.png`:
circular scope, black outside the aiming viewport and face/health/ammo/radar retained. This is
single-player evidence; no guessed split-screen ID added. Original capture companions copied to
`/tmp/army-disc-versions/pal/scope-user-capture/` before further captures could overwrite them.
Before/after replays: `scope-before.png`, `scope-fixed.png`. No captured game data tracked.

Follow-up user test: ground seams are no longer observed. No additional terrain patch was
introduced in this follow-up; keep the shared renderer fixes and reopen only on a new reproduction.

## Multiplayer follow-up

The reported black player-setup backdrop/gray Sarge/absent Vikki and input stall were reproduced
with two Maple controllers in both4:3 and widescreen. Missing PAL setup/rendering entries were
translated; the complete setup now renders correctly and both players respond. Final10000-frame
route covers bothA confirmations, map selection, split-screen match, pause and resume with zero
untranslated calls. GPU replay confirms both textured players/HUDs after resume. See
[PAL multiplayer findings](pal-multiplayer.md) for reference captures, diagnostics and the live
window verification limitation. No user setting needed; restart the updated PAL test build.


## Release preparation and user coverage (2026-11-06)

User confirmed the current visual/controller changes and a short Snow mission test:
level loads, helicopter audio plays, gameplay appears normal. Full first campaign
level was previously completed by the user. Remaining campaign coverage takes time.
User also confirmed that original 50 Hz and non-English startup selections crash.
These variants have no translations and are not supported by this preview.

Preparing v0.1.8-pal-preview.1 for Linux, with primary launcher PAL hash recognition
and a separately translated PAL child runtime. The NTSC runtime's own allowlist
still rejects PAL. Before routing, the three PAL file hashes are verified; argument
forwarding uses SDL process argv without a shell. Settings stay with the main
launcher, PAL uses a separate default VMU, and a custom VMU is preserved.

The bootstrap's strcat at0c010bbc appends the selected language to the Klink path.
Its exact call PR0c0353bc and destination8c118d70 identify the selection handoff:
r5 points into the language table8c09d5b0 (16-byte entries). Subsequent code reads
8c118f70 and appends 60Hz only when it equals60. A guarded preview hook substitutes
the original English string and sets that mode60. Other strcat callers are untouched.
The original startup UI remains, but its choices are overridden to English/60 Hz.
This is a support restriction, not translations of other languages/50 Hz.

Both final runtimes build with `DREAM_DEV_INTERPRETER=OFF`. All ten original
language/video choices now reach English60 at frame2000 with zero untranslated
calls (`tools/verify_pal_startup.py`, `/tmp/army-pal-preview-startup2`). Both USA
and PAL first-mission mouse baseline/turn checks pass on the release binaries.
The PAL script no longer presses A1050, which now skips a developer logo and
invalidates the original pre-skip menu schedule. Four-player native tests, the
actual profile UI test, revision/overlay-signature and tail-runner tests pass.
Packaged validation and publication are recorded in the release notes.

