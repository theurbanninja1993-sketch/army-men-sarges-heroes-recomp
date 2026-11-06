# PAL multiplayer player setup stall (2026-11-05)

User capture at guest1915/presented1882 shows a black setup backdrop, gray Sarge, absent Vikki,
and unresponsive input. Original companions preserved in `/tmp/army-disc-versions/pal/multiplayer-user-capture/`.
The image was inspected; no source capture files modified.

## Reproduction and cause

The headless harness initially had only portA attached, preventing Players2 selection. Those
single-pad `multi-repro`, `multi-setup`, `multi-native-6500` tests entered single-player/Bootcamp
and are not multiplayer evidence. Do not use their results to assess this issue.

Added opt-in `DREAM_TEST_PAD2=from:to:hexmask` diagnostic: attaches portB, neutral outside the
requested guest frames. A window preserves the diagnostic pad rather than replacing it with
physical enumeration; unset keeps existing host behavior. Existing --press drives player1.

Correct reproduction (fresh scratch VMU, RTCseed1000000):
`down@550,a@600,a@750,a@900,start@2900,a@3050,right@3900,start@4300`
with `DREAM_TEST_PAD2=0:0:0`, stop6500, dump6400. Native 4:3 and widescreen both stall after
entering frontend screen8; no TA is completed at6400. Their reports show60 untranslated calls
across17 entries starting0x0c0397a4, called from0x0c038114, plus continuation points through
0x0c03a83e. Only known unmapped write0x8bfff1c0 remains.

The same route through `--interpret` renders the complete normal setup: camo/green frame,
textured Sarge and Vikki, emblems and names. Reference `mp2-interp-6400.{ta,vram}`, rendered
`mp2-interp.png`; RAM confirms phase0/frontend screen8. This isolates a native translation gap,
not a widescreen-only correction or missing disc textures.

## Work in progress

Added all17 observed entries to `game/armymensh_pal_en60.toml` and rebuilding the enhanced
PAL target (`mp2-build.log`, final runtime relink `mp2-final-build.log`). Native setup now
renders all characters, textures, frame and emblems correctly in widescreen. At6500frames,
zero untranslated and only the known unmapped write remain. A9000-frame input run also passes
zero untranslated and shows player1 Ready, confirming the menu is responsive. Player2 requires
confirm/A rather than Start; a two-player A-confirmation/game-start test is running (`mp2-ready.*`).
Setup and input captures: `mp2-fixed-{setup,confirm}.{ta,vram,png,ram,log,txt}`.
All evidence is local/ignored or in `/tmp`; no game content distributed.

## Match progression

A-confirmation is required for each player. Player1 `a@6900`, virtual player2
`DREAM_TEST_PAD2=7100:7116:4` both become Ready and the default two-player map starts.
At10000frames RAM confirms mission phase3/frontend screen10. The captured split-screen
map has both radars and characters. Player1 Start opens its native pause panel (captured).
This route has one further observed interpreter continuation0x0c06a8b8; added to the profile
and final rebuild is running (`mp2-match-build.log`). Prior report `mp2-ready.txt` has1 fallback;
do not claim that report is zero untranslated. Remaining unmapped access is the known write.

Window/audio10000-frame run with both confirms and pause/resume is in progress (`mp2-window.*`).
Final zero-fallback match repeat remains after the build.

Final continuation build completed. `mp2-final`10000-frame run passes zero untranslated and only
known unmapped write; both confirms reach the normal Island map selector (captured/inspected).
A is required to select that map; Start alone does not launch it. A final match run adds
A8100, Start9000/pause and A9400/resume (`mp2-final-match.*`).

Live-window attempt: Wayland reported no displays; X11 initialized but advanced only219frames
in197host seconds, before multiplayer. The owned test was stopped and closed normally. It did
not validate multiplayer. Evidence `mp2-window-x11.log`, `mp2-window.txt`; GPU capture replay
and headless guest validation above pass. Do not describe this attempt as a passing window test.

## Final result

Final enhanced PAL binary rebuilt with the setup/rendering entries and continuation0x0c06a8b8.
`mp2-final-match` passes10000frames, exit0, **zero untranslated calls**, only known unmapped
write0x8bfff1c0. Both players confirm viaA, select the Island map, enter split-screen gameplay,
then player1Start9000 pauses and A9400 resumes. RAM at10000 is phase3/frontend10; frame9900
GPU replay was visually inspected: both textured characters, two viewports/radars/HUDs and no
pause overlay. Evidence `mp2-final-match.{log,txt,ram,ta,vram,png}`.

User-facing test: restart the same experimental PAL command and select multiplayer again.
This closes the reported setup stall in automated guest execution and GPU capture replay;
a fresh user gameplay check remains appropriate. No additional runtime setting required.

## Multiplayer sniper scope bars (2026-11-06)

User capture at guest3553/presented3520 identifies PAL split-screen scope TCW`0x10070900`:
a flat quad at native160,0..480,256, depth60001. Added this measured ID beside the single-player
`0x10070910` in `game/hud_overrides_pal_en60.ini`, reusing viewport-aware `bars=1` correction.
No renderer/executable rebuild required; reload by restarting the PAL port.

GPU before/after replay confirms black outside the scope across the aiming player's full width,
with circular scope and HUD retained. The scope/HUD interior and the entire second-player region
are byte-identical. The captured first viewport extends to nativey256 (renderedy384 at720high),
so compare the other player below384, not the nominal360half. No pixels change belowy380.
Original companions preserved `/tmp/army-disc-versions/pal/multiplayer-scope-user-capture/`;
replays `multiplayer-scope-{before,fixed}.png`. USA HUD file unchanged, no captured content tracked.
