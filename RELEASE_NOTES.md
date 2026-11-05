# v0.1.7 — USA disc compatibility

- Adds support for the USA GD-ROM executable with SHA-1
  `5f49f40a0a30fe240d8868025201326b266e369b`, including the verified USA CHD and CUE/BIN sets.
  This fixes the matching executable-hash error reported through RecompLauncher.
- Keeps support for the original CDI executable, SHA-1
  `51384b12a9de6d9a44caecaadafd29dca86807cf`.
- Reads three disc-address constants from the selected executable instead of baking in the CDI
  values. Both verified executables share the same translated instructions and hook addresses.
  The selected image supplies its own boot bytes even if a developer extraction is present.
- Preserves the USA GD-ROM's native Snow intro soundtrack. The CDI retains its added six-second
  helicopter cue, since that copy lacks the native stream.
- **PAL/Europe is not supported yet.** Its language/video-mode launcher and secondary executables
  need a separate translation and enhancement mappings. Unknown executable hashes remain rejected.
- Includes the existing v0.1.6 multiplayer radar and earlier widescreen fixes.

Linux and Windows downloads are included. USA CHD and CUE/BIN boot checks passed on Linux and
under Wine for the Windows build; all 14 first-mission mouse/camera checks passed on USA CHD.
Native Windows gameplay testing of this compatibility update remains pending.

Your own matching Dreamcast image is required. No disc images, extracted game assets, BIOS,
saves, captures, or generated game source are included. The executable contains the compiled
static translation. Matching source and dependency notices accompany the binaries.

**AI use disclosure:** development used extensive AI coding assistance for code, analysis,
debugging, tests, and documentation, directed and tested by the maintainer. Bugs may remain.
Project-specific original code is **GPL-2.0-or-later**; dependencies retain their own licenses.

# v0.1.6 — Multiplayer radar fix

- **Player 1's radar in split screen** now stays at the left edge with Player 2's. The
  character-select name/emblem position fallbacks no longer catch the top radar or its own
  arrow; overrides gained texture, size and texture-word range filters.
- Includes the v0.1.5 radar-marker fix and the v0.1.4 sniper-scope and character-select fixes.

# v0.1.5 — Radar marker fix

- **Radar enemy markers:** the character-select dark-panel override also matched the radar's
  small untextured enemy markers and centred them, leaving them outside the radar at 16:9.
  Overrides can now require a minimum piece size; the dark-panel rule requires a large panel,
  so the markers follow the radar's left-side anchor again.
- Includes the v0.1.4 sniper-scope and character-select fixes.

# v0.1.4 — Sniper scope and character-select fixes

- **Sniper scope:** keeps the 4:3 scope texture at its native shape, so the crosshair stays
  circular, and fills the extra width of the aiming player's viewport with black. In single
  player that is the whole screen; in split screen only the aiming player's half or quadrant.
  The game's own HUD still draws over the black.
- **Character select:** the left name and emblem stay inside the centred green frame on the
  first visit and after returning from a match.
- Includes the v0.1.3 fixes: player set-up text/dark panel inside the green frame, and correct
  split-screen culling.

# v0.1.3 — Multiplayer widescreen fixes

- **Player Set-Up screen:** the left name/emblem and the dark panel now stay inside the centred
  green frame, and the character/gun are clipped to the panel border.
- **Two-player split screen:** the game's own viewport aspect is scaled by the widescreen factor
  instead of being overwritten with 16:9. Objects now stay visible to the edge of each half.
- Single player at 16:9 is unchanged.

# v0.1.2 — Windows x64 release

Adds a native Windows x64 build alongside the Linux build. Same game code and features.

- Windows 10/11 x64, a Vulkan-capable GPU and current drivers. SDL3 is bundled and the Microsoft
  C/C++ runtime is linked statically, so no redistributable is needed.
- Validated on Windows 11: level 1 through level 2, mouse and keyboard, and both Classic and
  Modern controller modes.
- Download `army-men-sarges-heroes-recomp-v0.1.2-Windows-x64.zip` on Windows, or
  `army-men-sarges-heroes-recomp-v0.1.2-linux-x86_64.tar.gz` on Linux.
- `recomplauncher.json` is included in both; the same setup, settings and saves work on either
  platform. Windows builds are console programs, so a console window stays open with the log.

# v0.1.1 — RecompLauncher support

Same game build as v0.1.0, packaged for RecompLauncher. The release now includes
`recomplauncher.json`, so RecompLauncher installs it, asks for your own Dreamcast image and
starts the game without the port's own launcher. Window mode, resolution, aspect ratio, frame
rate, frame timing, HUD layout, controller mode, mouse camera and loud sounds appear on the
game's page.

Supported images: `.cdi`, `.gdi` (keep its track files beside it), `.cue` with its `.bin`, or
`.chd`. Needs the USA release, product T-9715N V1.001. RecompLauncher cannot verify a Dreamcast
container itself, so the game checks the boot executable's SHA-1 at start and reports a wrong
release or revision.

Linux x86_64, glibc 2.39 or newer, a Vulkan GPU and working drivers. SDL3 is bundled.

# v0.1.0 — First Linux release

An unofficial native Linux x86_64 port of the Dreamcast version of Army Men: Sarge's Heroes.

**Windows version coming soon.** Windows and Steam Deck support have not yet been validated.

## Getting started

Download `army-men-sarges-heroes-recomp-v0.1.0-linux-x86_64.tar.gz`, extract it, and run
`./army-men-sarges-heroes-recomp`. Choose **Load Game**, select your own matching Dreamcast
CDI, GDI, CUE/BIN, or CHD image, then choose **Start Game**. Keep track files with their GDI/CUE.

Requires glibc 2.39 or newer, a compatible libstdc++ runtime (Ubuntu 24.04 or newer), a Vulkan GPU,
and working graphics drivers. SDL3 is bundled. Other distributions have not been individually
tested. The original tested revision is T-9715N V1.001; the launcher checks its boot SHA-1.

## Included

- Mouse camera with horizontal-only/full modes and separate normal/aim sensitivities.
- Classic and Modern controller modes with a right-stick camera and modern shooter bindings.
- Widescreen and HUD options, resolution scaling, frame interpolation, and saved settings.
- Mouse support in supported native menus and keyboard level-password entry.
- Optional developer-logo skipping and a helicopter audio enhancement for the Snow introduction.
- Direct CDI loading in the launcher, in addition to GDI/CUE/CHD.

Press Escape or controller Select/Back for port settings. Some changes require a restart.
If interpolation artifacts occur, select 60 FPS and restart. If brightness flickers with adaptive
sync enabled, try disabling adaptive sync/VRR for this application.

## AI use disclosure

Development used extensive assistance from AI coding agents for code, reverse-engineering
analysis, debugging, tests, and documentation. The maintainer directed the work and tested
gameplay. AI-assisted work can contain mistakes; bugs and incomplete behavior may remain.

## License and game data

Project-specific original code is GPL-2.0-or-later. Dependencies retain their own license terms.
The source archive includes the modified framework and the dependency sources used for the build;
license notices are included in both packages. SHA256SUMS covers the two release archives.

No disc images, extracted assets, BIOS files, memory cards, gameplay captures, or generated game
source are included. The executable contains the compiled static translation and requires the
player's own matching disc image. This project is not affiliated with the original rights holders.
