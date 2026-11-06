# Skippable developer logos (2026-10-21)

Press the bound Dreamcast Start or confirm/A button to skip the current Midway
or Saffire logo. Default keyboard bindings are Enter (Start) and Space (A).
Controller Start and the normal A/confirm button work in Classic and Modern.
One new press skips one logo; holding a button does not repeatedly skip.

Implementation: `src/startup_logos.cpp`, registered in `game/CMakeLists.txt`,
with the `logo_status@entry` hook at `0x0C12376E` in `game/armymensh.toml`.
No disc/movie assets are modified.

## Native control flow

- `fn_0c072f78` plays a movie by ID. ID 1 is `MIDWAYLOGO.SFD` at
  `0x8C0F0220`; ID 2 is `SAFFIRELOGO.SFD` at `0x8C0F022F`.
- Movie state is at `0x8C0F01E0`: filename at +12, decoder handle at +16,
  movie ID at +56. IDs 3/4 are the making-of/testers extras and are excluded.
- `fn_0c071f24(1)` polls input in each movie-loop iteration. Player one's
  active-high newly pressed button mask is at `0x8C1FDBF8`, with A=0x04
  and Start=0x08. These already respect host bindings and menu input blocking.
- The next virtual status query (vtable +32) resolves to `fn_0c12376e`,
  called with PR `0x0C073074`. Only this caller and the two logo ID/filename
  combinations are intercepted. A skip returns status 3, which the original
  loop recognizes as finished and follows with stop/destroy/audio restore.
- A discarded prototype wrote the movie-control flag at +0. That flag persisted
  into the next movie, causing it to end too. The final status hook leaves that
  flag intact and requires a fresh button edge for each logo.

## Build relocation

After reboot the drive mounted at `/run/media/bubba/1TB SSD1`, while the CMake
cache retained `1TB SSD`. The cache mount prefix was corrected (backup:
`/tmp/army-build-cache-before-remount.txt`), then CMake regenerated the build
using the existing downloaded dependencies with FetchContent disconnected.
The complete game was rebuilt at the current mount. The launcher still selects
`build/game/army-men-sarges-heroes-recomp`, with the updated saved disc path.

## Verification

- Headless deterministic runs with `--rtc-seed 1000000 --max-frames 1000`:
  `--press start@400,a@600` and the reverse `a@400,start@600` each report
  both logos skipped. The decoder handle is released and the persistent movie
  flag remains zero after both skips.
- A continuous Start hold from frame 400 through frame 500 reports only Midway
  skipped. At frame 500 Saffire still has a live decoder and control flag zero.
- With no button input, all 16 MB of RAM at frame 1000 are byte-for-byte
  identical to the executable preserved before this feature was added.
- A Vulkan windowed startup check skips both logos and visibly reaches the title
  screen. Normal loading and the legal text screen remain in the startup flow.
- All checks report zero untranslated targets and only the already-known benign
  unmapped write at `0x8BFFF1C0`. No new runtime faults.
- Final checks/logs and RAM snapshots are under `/tmp/army-logo-final-*`;
  window screenshots/log are in `/tmp/army-logo-final-window/`.


Mount follow-up: the user configured fstab to mount the project drive at
`/mnt/1TBSSD`. Launcher disc and CMake cache paths are now migrated there,
and CMake regeneration plus a one-frame startup check succeeded. Use:

```sh
cd "/mnt/1TBSSD/Projects/Sarge's Heroes Recomp/work" && python3 src/dreamcomp/tools/dc.py run ports/army-men-sarges-heroes-recomp --window
```

The executable was unchanged by this path update.

## PAL English60 (2026-11-05)

The experimental PAL controls target now includes the same source, using `game_layout.h` for
executable-specific addresses. English60 status entry0x0c12b00e, callerPR0x0c073724, movie
state0x8c0f0ab0, filenames0x8c0f0af0/0x8c0f0aff, logical new buttons0x8c1fe738. Only the
English60 main executable is hooked; the language/video bootstrap retains its native behavior.
Stock PAL hooks supply a no-op for the shared guest translation.

Headless integration tests pass, all zero untranslated:

- Bootstrap `down@550,a@600,a@750,a@900`, then Start1300/A1600 skips both logos. By frame2000
  decoder handle is0 and persistent flags0.
- Continuous Start1300..1500 skips Midway only. Saffire has a live decoder at frame1500;
  persistent flags remain0.
- No skip presses: full16MiB RAM at frame2000 is byte-identical to the pre-hook binary, using
  RTCseed1000000 and matching fresh scratch VMUs.
- The earlier test bootstrap also pressed A1050, which is already during Midway playback and
  legitimately skips it. That sequence is not the no-input fixture. Corrected evidence is
  `/tmp/army-disc-versions/pal/logo-v2-{old,none,both,held}.{ram,log,txt}`.

Window/audio verification passes2200frames: both logos skipped, native startup proceeds to the
title screen (frame2100 visually inspected), zero untranslated and zero failed texture decodes.
This test is unthrottled, so excess audio samples are intentionally dropped; it is not an audio
quality benchmark. Evidence `logo-window.{log,txt}`, `logo-window-2100.{ta,vram,png}`.
