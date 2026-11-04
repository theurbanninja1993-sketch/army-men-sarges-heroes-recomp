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
- The dark panel behind both player boxes is untextured and spans the upper and lower rows.
  Its centre (y~271) is below the background plane's, so `rect=30,250,614,448 anchor=center tcw=0`
  moves it with the frame without touching the camo backdrop.
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

## Sniper scope overlay

The sniper scope is one flat textured quad. In the user's captures:

| Case | TCW | Quad (native 640x480) |
| --- | --- | --- |
| Single player | `0x1006e910` | `0,0,640,480` |
| Two-player top half | `0x1006e900` | `160,0,480,256` |

Single player drew it full-screen, so the renderer stretched the 4:3 texture to 16:9 and made the
crosshair oval. In split screen it stayed 4:3 and the game picture showed on the extra width.

Fix: a `bars=1` override (`rect=0,0,640,480 anchor=center tcw=0x1006e910,0x1006e900 full=1`)
keeps the scope at its native 4:3 shape, and the HUD pass fills the rest of the aiming player's
viewport with opaque black. The viewport is the full screen for single player and the half or
quadrant that contains the scope quad for split screen. The bars are inserted just in front of the
picture (scope depth - 1) so the game's own HUD (face, radar, ammo) still draws over them.

Verified with the single and multi captures: the crosshair is circular, the game picture outside
the 4:3 scope is black, the other player's half is untouched, and the HUD remains visible.

## Left name and emblem texture words

The left player's name and emblem can use different texture words than the first capture (per
profile, and again after returning from a match to the set-up screen). Position-based overrides
catch both bands with `anchor=center`:

```
rect=0,100,300,188 anchor=center   # name lines
rect=0,188,300,260 anchor=center   # emblem
```

The name band stops above the emblem and below the header, so it does not split "Normal" or
touch the right player. Both bands follow the centred green frame regardless of the texture
word the game picks for that visit.

## Radar enemy markers

The in-game radar map is a textured 2D element; its enemy markers are small untextured quads
(4x4 to 14x14) in the same screen area. The player-setup dark-panel override
(`rect=30,250,614,448 anchor=center tcw=0`) also caught those markers, centring them while the
radar itself was left-anchored, so at 16:9 they drifted outside the map.

`min_w` / `min_h` now scope an override to pieces at least that size. The dark-panel override uses
`min_w=100 min_h=100`: the 580x350 panel matches, the small markers do not. The markers then use
the normal left-third rule and move with the radar. Verified with the HUD capture.

### Follow-up: the top player's radar in multiplayer

The position-based name/emblem fallbacks had no texture filter, so in split screen they also
caught the top player's radar (its centre falls in the name band) and the radar's own arrow (a
small textured sprite). The top radar was centred while the bottom player's radar stayed
left-anchored.

The fallbacks are now scoped: `textured=1`, `max_w=40 max_h=40` for the name band, `min_w=20
min_h=20` for the emblem, and `tcw_min=0x04000000` so only the font texture pages match. The radar
(144x144) is too large, and its arrow uses a sprite page below the text range. Verified with the
multiplayer HUD capture: both radars sit at the left edge and their markers stay inside.
