# Army Men: Sarge's Heroes — Recompiled (Dreamcast)

[![Get it on RecompLauncher](https://recomplauncher.com/assets/press/launcher/badge-dark.svg)](https://recomplauncher.com/game/army-men-sarges-heroes-recomp)
[![Verified developer on RecompStore](https://recompstore.com/assets/press/verified-dark.svg)](https://recompstore.com/dev/theurbanninja1993-sketch)

An unofficial native PC port of the Dreamcast version of **Army Men: Sarge's Heroes**, built
with [Dreamcomp](https://github.com/danielgomesvieira2000/dreamcomp).

**Linux and Windows x64 releases available.** The Windows build was validated on Windows 11:
level 1 through level 2, mouse and keyboard, and Classic and Modern controller modes. Steam Deck
has not yet been validated separately.

## Play

### Linux

1. Download the Linux x86_64 archive from [Releases](https://github.com/theurbanninja1993-sketch/army-men-sarges-heroes-recomp/releases) and extract it.
2. Run `./army-men-sarges-heroes-recomp` in the extracted directory. If your file manager asks,
   allow the executable to run.
3. In the launcher, choose **Load Game** and select your own Dreamcast disc image:
   **CDI, GDI, CUE/BIN, or CHD**. Keep a GDI/CUE's accompanying track files together.
4. Choose **Start Game**. The launcher remembers the disc location and your settings.

### Windows

1. Download the Windows x64 archive from [Releases](https://github.com/theurbanninja1993-sketch/army-men-sarges-heroes-recomp/releases) and extract it somewhere writable.
2. Run `army-men-sarges-heroes-recomp.exe`. If Windows asks, allow it.
3. In the launcher, choose **Load Game** and select your own Dreamcast disc image:
   **CDI, GDI, CUE/BIN, or CHD**. Keep a GDI/CUE's accompanying track files together.
4. Choose **Start Game**. The launcher remembers the disc location and your settings.

The Windows build is a console program, so a console window stays open with the log. Keep it while
testing or reporting an issue.

You must supply your own game. The port supports these verified **T-9715N V1.001** executables:

| Image contents | Boot executable SHA-1 |
|---|---|
| USA GD-ROM layout (including GDI/CHD dumps) | `5f49f40a0a30fe240d8868025201326b266e369b` |
| Previously supported CDI copy | `51384b12a9de6d9a44caecaadafd29dca86807cf` |

The executable hash, rather than the image extension or filename, determines compatibility.
PAL and other revisions are not yet supported. Different or modified executables are rejected.
The image stays on your computer.
No disc image, extracted assets, BIOS, saves, or generated game source are distributed here.
The release executable includes the compiled static translation and still requires your disc.

### Requirements

- **Linux:** x86_64, glibc 2.39 or newer (Ubuntu 24.04 or a compatible/newer distribution), a
  Vulkan-capable GPU with working drivers, and a desktop session.
- **Windows:** Windows 10/11 x64 and a Vulkan-capable GPU with current drivers. SDL3 is bundled and
  the Microsoft C/C++ runtime is linked statically, so no redistributable is needed.
- Keyboard and mouse or a gamepad.

### RecompLauncher

The release includes `recomplauncher.json`, so [RecompLauncher](https://recomplauncher.com/)
can install it, ask for your own game image and start the game directly. Pick your own
`.cdi`, `.gdi` (keep its track files beside it), `.cue` with its `.bin`, or `.chd`.
RecompLauncher writes the path into the game's settings, skips the port's own launcher, and
shows the common video and control settings on the game's page.

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
