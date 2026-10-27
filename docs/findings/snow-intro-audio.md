# Snow drop-off helicopter audio (2026-10-22)

## Requested enhancement

The user confirmed in Flycast that the original Snow opening cinematic is silent,
then requested adding helicopter audio anyway. This is a port enhancement, not
an assertion that the Dreamcast original had this sound.

The supplied disc does not contain the requested `CIN8_1.STR` soundtrack.
Its `LEVEL08.UFS` does contain an ELF `snowc0.o` with a native sound-effect cue:
`_CnmSndFx_Snow_Cnm00`, .data offset 0x32c, size 40. The cue covers 0–4000 ms,
names sample 23 at volume 100, and enables positional attenuation at world origin.
The cinematic lasts 10533 ms. The Dreamcast cinematic code additionally mutes
ordinary sound effects while its streamed soundtrack plays; simply replacing the
sample initially produced a valid sound handle but still-silent recorded audio.

## Implementation

Port source: `src/snow_intro_audio.cpp`. No extracted audio is stored in the port.

- `snow_intro_audio@entry` wraps native cinematic updater `0x0c025820`. Only
  level 8, cinematic ID 0, duration 10533 and the exact known single sound cue
  qualify. Guest pointer ranges and cue fields are checked before editing.
- Replace sample 23 with sample 33, the looping helicopter engine used by native
  vehicle code at `0x0c04f378` (also cinematic vehicle type 16 at `0x0c023bd6`).
  Clear the cue's positional flag at +16, so its distant origin does not mute it.
  Set its end time to 6000 ms, retaining the
  authored volume and native handle at +36.
- `snow_intro_play@entry` wraps low-level sound start `0x0c01dcb4`. Only the
  native SFX wrapper caller `0x0c055c4c`, sample 33, a currently muted playback
  volume and this restored Snow cue within its 0–6000 ms interval qualify.
  Set that call's volume from the SFX level saved by the cinematic at
  `0x8c17e4e8`: native curve `50 + 5 * level` for this volume-100 cue.
  Level 0 stays silent. Global sound settings and the cinematic mute remain intact.
- Existing native cue cleanup stops/releases the sound at six seconds and when
  Start skips the cinematic. Native cleanup restores the saved sound-effects level.

Hooks are registered in `game/armymensh.toml`; the source is in `game/CMakeLists.txt`.

## Verification

Temporary diagnostic code selected Snow at vblank 3705 in a deterministic
headless new-game run. It used separate settings and VMU files under `/tmp`;
the user's save was not used. All diagnostic selection, logging and disable
switches were removed before rebuilding the playable executable.

Normal, skipped and muted cases ran to frame 5100 with WAV recordings and RAM
snapshots. Logs/audio/RAM: `/tmp/army-heli-{audible,skip,muted}.{log,wav,ram}`.

- Normal: cue handle `0x8c079550` active at cinematic 1982/3984 ms, cleared by
  5986 ms. Audio RMS about 1700–1800 (16-bit PCM), peak below 9600, during the
  cue; 76.5–78 guest seconds are exactly silent after it stops. Before the volume
  correction the same interval was completely silent.
- Start at frame 4400: cinematic exited and handle cleared before frame 4440;
  normal gameplay audio resumes. No rotor handle remains.
- SFX level 0 before cinematic initialization: the whole intro cue interval stays
  exactly silent. Saved/current effects levels both remain 0 afterward.
- Normal/skipped saved/current effects levels both restore to 7 after exit.
- All runs: zero untranslated calls and only the previously known unmapped write
  to `0x8bfff1c0`. No new runtime faults.
- Final playable build: `/tmp/army-heli-final-build.log`; temporary test controls
  are absent from shipping sources. Listening/gameplay confirmation is pending.


## Follow-up: continue through the fly-away

The user confirmed the sound is appropriate and requested continuing it after
Sarge jumps out. The authored 4000 ms cue ended at that jump. The hook now extends
the cue to the cinematic header's full 10533 ms duration, including the fly-away;
its muted-volume exception uses the same extended interval.

A native replay verifies the handle stays active at 9990 ms and clears when the
scene exits. Recorded audio RMS stays above 1600 in the previously silent tail
(77–78, 80–81 and 82–82.5 guest seconds). Zero new runtime faults. Existing native
skip cleanup and volume handling remain in use. The normal game was rebuilt after
removing the temporary timing/level-selection callback; user saves were not used.
Artifacts: `/tmp/army-heli-extended.{log,wav,ram}` and
`/tmp/army-heli-extended-final-build.log`. Extended timing awaits gameplay confirmation.

## Follow-up: six-second cutoff (2026-10-22)

The user revised the requested duration to about six seconds, when the helicopter
leaves the screen. The final hook writes 6000 ms and uses the same limit for its
volume exception. Native stop/skip handling remains in use. The playable game
was rebuilt. The longer replay above documents the earlier revision; listening
confirmation of the final six-second cutoff is pending.
