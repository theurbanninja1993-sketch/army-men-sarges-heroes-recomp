# Native menu mouse and keyboard input (2026-10-22)

## Controls and coverage

`src/native_menus.cpp` adds pointer input to the title Start prompt, startup
Create / Continue without saving prompt, main menu, Options, Input Code, and
native generic in-game panels including the pause menu. Hover selects; left-click
activates; right-click backs out. Options support clicking left/right or using
the wheel to change values. Players cycles on click, and Playmode changes with
the wheel. Existing controller navigation remains available.

Input Code accepts physical letter keys (case insensitive), Backspace, Delete
(clear), and Enter. Only the game's native consonant alphabet is accepted, with
its existing 12-character limit. Clicking the on-screen keys also works. Submission
uses the original validator and callbacks, rather than implementing passwords in
the port. Gameplay bindings for typed letters are suppressed in this screen.

The pointer is released in these menus and recaptured in gameplay if mouse
capture was enabled. A held click cannot become a gameplay shot/aim on Resume.
Coordinates account for the centered 320x240 artwork viewport in widescreen,
window scaling, and letterboxing. Port settings overlays retain event priority.
Actions are discarded on screen changes and focus loss.

Special multiplayer setup screens and other VMU error/overwrite dialogues have
not been mapped; their existing keyboard/controller navigation is retained.

## Engine integration

`patches/dreamcomp-native-menu-input.patch` adds HostControls pointer-mode and
window-coordinate callbacks, main-thread SDL window-size lookup, and mouse
capture/binding gating. Apply after the prior local engine patches. The port
extension gets SDL events without opening a host overlay or pausing the guest.

Native state addresses: phase 0x8c18ed1c; frontend screen 0x8c0eb1d0;
main/options/password substate 0x8c1a53c0; save dialogue state 0x8c1a3c28.
Main/options selected rows are 0x8c1a4de0/0x8c1a4de8. Password buffer/length
are 0x8c1a5338/0x8c1a5350, with row/column at 0x8c1a5358/0x8c1a5360.
Generic panels use the active player at 0x8c18cc70 and pointer table
0x8c0cfda0; entries provide native row positions and action/value semantics.
Frontend detection must be gated by phase because its index remains stale
through gameplay.

## Verification

The playable executable and optional `armymensh_menu_tests` target were rebuilt.
Assertion-enabled SDL event tests cover widescreen pointer placement and ignored
side-bar clicks, password editing and limits, gameplay isolation, pause rows,
startup Create/Continue choices, and title Start. Tests use RAM snapshots only:

```
build/tools/armymensh_menu_tests /tmp/army-menu-main.ram \
  /tmp/army-menu-code.ram /tmp/army-menu-pause.ram /tmp/army-menu-startup.ram
```

Live window tests entered BLZZRX, corrected it to BLZZRD with Backspace,
submitted to the native Snow Mission validator, opened Options and changed SFX.
A second live run clicked Resume in the native pause menu and verified pointer
capture transitions back into gameplay. Zero untranslated calls; only the known
unmapped write at 0x8bfff1c0. All runs used separate settings/cards under /tmp.
Logs: /tmp/army-menu-live.log and /tmp/army-menu-pause-live.log.
Optional ARMY_MENU_SCRIPT and ARMY_TRACE_MENU diagnostics are inactive by default.

## Capture incident

The first live test mistakenly ran a numbered capture in the port directory,
overwriting capture-000's current native PPM/TA/VRAM and text metadata. Its original
interpolated PPM and previous TA survived, along with preview and fixed images.
Surviving originals are preserved under shots/snow-seam-original/. The original
current-frame TA/VRAM could not be recovered. Other numbered captures are unchanged.
Later runs use /tmp working directories; never run automated numbered captures
in the user's capture directory.

## Startup detection correction (2026-10-22)

The user reported capture staying enabled and clicks doing nothing after the
initial build. Its detection assumed frontend phase 7, and mistakenly treated
screen 12 as the title. Normal startup runs phase 0 with title screen 4; screen
12 is a separate end screen. Detection now accepts frontend phases 0 and 7 and
identifies title screen 4. Gameplay phase gating remains, so a stale frontend
index cannot release capture or intercept gameplay input.

A normal-startup run without --press reproduced capture staying on in the old
code. After correction, actual SDL relative mode changes 1 to 0 on title entry
at frame 1756. SDL events injected through SDL_PushEvent (not direct menu actions)
click both title stages, Continue without saving, and main-menu Input Code.
Typing BLZZRX, Backspace, D, Enter produces the native Snow Mission result
(level 8, result pointer 0x8c0ee129). This verifies the window event queue,
extension event filter, coordinate mapping, button polling, and native validator
together. Logs/RAM: /tmp/army-menu-sdl-fixed.{log,ram}; script:
/tmp/army-menu-sdl-script.txt. The optional ARMY_MENU_SCRIPT diagnostic supports
sdl_click/sdl_text/sdl_erase/sdl_enter for this path. No user save was written.
Assertion tests now cover phase 0 main/title detection as well as phase 7.
