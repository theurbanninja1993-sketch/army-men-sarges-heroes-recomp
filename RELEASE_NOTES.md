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
