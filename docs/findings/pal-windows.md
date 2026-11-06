# PAL English60 Windows test build (2026-11-06)

The PAL runtime and dispatcher now cross-compile for Windows x64 with the existing
clang-cl/lld-link and xwin toolchain, SDL3 3.4.18 and frontend enabled. Configure
`ARMYMENSH_PAL_SUPPORT=ON`, `ARMYMENSH_PAL_PROBE=OFF`, `DREAM_DEV_INTERPRETER=OFF`.
Put the native Linux dream-translate on PATH for cross-build custom commands.
Both game executables are built explicitly; existing default NTSC builds remain available.

`tools/package_release_windows.py --pal` includes the second executable, bootstrap
config and PAL HUD rules using an explicit allowlist. The launcher already chooses
`.exe` on Windows and forwards the verified PAL image, arguments/settings and VMU
selection using SDL_CreateProcess. Missing-runtime messages and RecompLauncher
metadata now refer to the matching platform package. No other language or 50Hz
translation has been added; all stock choices remain restricted to English/60Hz.

## Checks completed under Wine

Isolated prefix: `/tmp/army-pal-windows-tests/prefix`; no existing user prefix changed.

- Both PE binaries link only SDL3, Vulkan and Windows system DLLs. No separate MSVC
  C/C++ runtime DLL imports; the runtime is linked statically. SDL3.dll is bundled.
- Windows-built USA and PAL four-player native camera tests pass: inheritance,
  mixed overrides, camera ownership, live changes, pause, release and persistence.
- Windows SDL/Vulkan four-virtual-controller test passes actual host input routing:
  contextual buttons, independent right sticks, overlay blocking and disconnects.
- The primary Windows launcher routes PAL correctly with paths containing spaces
  and apostrophes. All10 original language/video choices load English60 with zero
  untranslated calls and the expected secondary-executable entry target.
- Actual PAL first-mission mouse baseline/turn and Classic/Modern right-stick
  scenarios pass against the Windows executable, with zero untranslated calls.

Logs: `/tmp/army-pal-windows-{config,build}.log`, and
`/tmp/army-pal-windows-tests/{startup,pal-mouse,pal-controller}`. The temporary
Wine runner converts Linux scratch paths to the prefix's Z: root mapping; game
arguments remain ordinary Windows paths in both parent and PAL child.

Wine verifies compatibility here; it does not establish native Windows gameplay
or driver performance. Final extracted-ZIP checks are recorded in the handoff.

## Native Windows acceptance

Extract the full test ZIP, run army-men-sarges-heroes-recomp.exe, and select the
verified PAL disc. Check first mission/FMV audio, mouse/RMB aim, Modern right
stick, Scorch marker, scope shape, pause/settings menus and saving/loading a PAL
card. Check NTSC still launches, plus multiplayer/player-specific overrides when
controllers are available. Do not announce native Windows PAL support until a
Windows playtest has confirmed the package. This build has not been published.
