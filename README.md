# Army Men: Sarge's Heroes — Recompiled (Dreamcast)

An unofficial native PC port of the Dreamcast version of **Army Men: Sarge's Heroes**, built
with [Dreamcomp](https://github.com/danielgomesvieira2000/dreamcomp).

**Linux release available. Windows version coming soon.** Windows builds and Steam Deck support
have not yet been validated.

## Play

1. Download the Linux x86_64 archive from [Releases](https://github.com/theurbanninja1993-sketch/army-men-sarges-heroes-recomp/releases) and extract it.
2. Run `./army-men-sarges-heroes-recomp` in the extracted directory. If your file manager asks,
   allow the executable to run.
3. In the launcher, choose **Load Game** and select your own Dreamcast disc image:
   **CDI, GDI, CUE/BIN, or CHD**. Keep a GDI/CUE's accompanying track files together.
4. Choose **Start Game**. The launcher remembers the disc location and your settings.

You must supply your own game. This build requires the matching Dreamcast executable revision
**T-9715N V1.001**, boot SHA-1 `51384b12a9de6d9a44caecaadafd29dca86807cf`.
Different releases or modified boot executables may be rejected. The image stays on your computer.
No disc image, extracted assets, BIOS, saves, or generated game source are distributed here.
The release executable includes the compiled static translation and still requires your disc.

### Linux requirements

- x86_64 Linux, glibc 2.39 or newer (Ubuntu 24.04 or a compatible/newer distribution).
- A Vulkan-capable GPU with working drivers and a desktop session.
- Keyboard and mouse or a gamepad. SDL3 is included in the binary package.

## Features

- Native mouse camera with horizontal-only/full camera modes and separate camera/aim sensitivities.
- Classic and Modern controller modes, including right-stick camera control in Modern mode.
- Expanded widescreen, HUD placement options, resolution scaling, and frame interpolation.
- Mouse navigation in supported game menus and keyboard entry for level passwords.
- Optional developer-logo skipping and added helicopter audio for the Snow introduction.
- A launcher with disc validation and saved graphics, sound, and control settings.

Press **Escape** or controller **Select/Back** for the port settings. The game's Start button
remains its pause/menu button. Some settings require a restart, as marked in the menu.

### Modern controls

| Action | Keyboard/mouse | Modern controller |
|---|---|---|
| Camera | Mouse | Right stick |
| Move | W/S; A/D strafe | Left stick forward/back |
| Strafe | A/D | Hold RB and move left stick sideways |
| Fire | Left mouse | RT |
| Aim | Hold right mouse | Hold LT |
| Sniper zoom while aiming | W/S | D-pad up/down |
| Quick zoom while aiming | Shift | RB |
| Change weapon | See Controls settings | X |
| Prone | See Controls settings | B |

Classic mode retains the original controller mappings. Bindings and sensitivities can be changed
in the launcher or port settings.

### Troubleshooting

If interpolation produces visual artifacts, select 60 FPS and restart. If brightness flickers,
try disabling adaptive sync/VRR for the application; this affected testing on the development
system. Report the level, settings, hardware, and reproduction steps in GitHub Issues.

## Build from source

See [BUILDING.md](BUILDING.md). The modified Dreamcomp source used by this port is included in
`dreamcomp/`; historical patches are retained for reference and must not be reapplied to it.
Building requires your own matching game image. Generated translation and extracted game files
are local build outputs and must never be committed or uploaded.

## AI use disclosure

This project was developed with extensive assistance from AI coding agents, including code,
reverse-engineering analysis, debugging, tests, and documentation. The maintainer directed the
work and tested gameplay. AI assistance can introduce mistakes; this release may still have
bugs or incomplete behavior. Please evaluate it on that basis and report reproducible issues.

## License and credits

Project-specific original code is licensed under **GPL-2.0-or-later**, as decided at the start
of this project. See [LICENSE](LICENSE) and [LICENSE-NOTICE.md](LICENSE-NOTICE.md).
Dependencies retain their own licenses; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Thanks to the Dreamcomp, dream-recomp, Flycast, SDL, RmlUi, FreeType, and libchdr contributors.
This is an unofficial fan project, not affiliated with or endorsed by the game's rights holders.
The license does not grant rights to the original game, its assets, names, or trademarks.
