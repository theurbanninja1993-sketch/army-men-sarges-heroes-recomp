# Multiplayer widescreen: player setup text and split-screen culling (2026-11-03)

Two issues reported after the first Windows release, both only in widescreen multiplayer.

## Player setup text split across the screen

The story-prose HUD override in `game/hud_overrides.ini` was:

```
rect=0,180,640,320 anchor=center tcw=0x04060010
```

The player setup screen draws "Green Sarge Normal" and "Tan Vikki Normal" with the same
resident-font texture word (`0x04060010`). The left line's glyphs sit at y=172..187: the tall
"N"/"l" have their centres at y=179.5 and the lowercase letters at y=180.5. The override's
`y0=180` therefore caught only the lowercase letters, forcing them to `anchor=center` while the
tall letters stayed on the automatic rule. The line split into three anchors: "N" left,
"orma" centre, "l" right.

Fix: start the story override at y=190, below the player setup lines (y1=187) and above the
story prose's first line (y0=200). No code change.

Verified with the 16:9 TA capture replay and a live 16:9 run: the left panel now reads
"Green Sarge Normal" intact, and the story card still centres as before.

## Player set-up content inside the green box

The full-width green frame is one HUD element. In the `edges` layout it is anchored to the
centre, but the left panel's name and emblem were auto-anchored to the screen's left edge, and
the panel's user tile clips (`32,96,320,256` / `320,96,608,256`) stayed at the native 4:3
rectangle. The text/emblem sat outside the centred frame, and the clipped character/gun could
draw past the panel border.

Fix:

- `game/hud_overrides.ini`: force the left panel's name and emblem to `anchor=center`
  (`rect=0,100,320,260` with `tcw=0x04060010,0x0007bf10`), matching the centred frame.
- `dreamcomp/src/hud.cpp`: when a polygon's user tile clip lies inside a HUD element, move the
  clip's left/right edges with that element's anchor and `k`. Offscreen model viewports and the
  background are outside every HUD element and stay untouched.

Verified with the 16:9 capture replay and a live 16:9 run: the left text and emblem sit inside
the left panel, and the character/gun are clipped to the panel border.

## Objects culled before the split-screen viewport edge

The game stores the viewport aspect at `camera + 0xe8`. The port used to overwrite it with the
full-screen target aspect (`16:9` = 1.7778). That is correct for single player, where the game's
own value is 4:3 (1.3333), but wrong for a two-player top/bottom split:

| Layout | Game's viewport aspect | 16:9 widescreen value |
| --- | --- | --- |
| Single player, 4:3 | 4/3 = 1.3333 | 16/9 = 1.7778 |
| Two players, top/bottom, 4:3 | 8/3 = 2.6667 | 16:9 top/bottom = 3.5556 |

The port wrote 1.7778 for both. The split viewport is 854x240 at 16:9 (aspect 3.558), so the
game culled at the narrower 1.7778 bounds and objects vanished before the visible edge.

Fix in `src/widescreen.cpp`: keep each camera's own aspect and scale it by the widescreen
factor (`target / 4:3`), instead of replacing it. The game's original value is refreshed
whenever it changes, so single-player/split-screen transitions and reused camera addresses work.

```
original 1.3333 * 1.3333 = 1.7778   single player
original 2.6667 * 1.3333 = 3.5556   two-player top/bottom
```

Evidence (`ARMY_TRACE_ASPECT=1`):

```
single player: aspect camera=8c1a06e0 original=1.33333 scaled=1.77777 target=1.77777
multiplayer:   aspect camera=8c1a1010 original=2.66666 scaled=3.55555 target=1.77777
```

The 16:9 two-player frame is 854x240 per viewport (3.558), matching the scaled value.

## Reproduction

The player setup screen is reached from MAIN MENU: set Players 2, Playmode 5 TO WIN, start, then
confirm both players. The headless test used a development-only pad B hook (since removed) to
press Start/A on port B. The culling value can be checked without gameplay by watching
`ARMY_TRACE_ASPECT=1` and entering a two-player match.

## Follow-up

The HUD override change is data-only and safe for every screen. The culling fix preserves the
single-player 16:9 result and the 4:3 original. Native multiplayer confirmation on Windows is
pending.
