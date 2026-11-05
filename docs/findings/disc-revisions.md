# Disc revision comparison and USA GD-ROM support (2026-11-05)

## Inputs and identity

All images remain local outside the port repository. No extracted executables, game files,
generated translations, or capture data are added to Git. Comparisons used the runtime's own
CDI/GDI/CUE/CHD readers, full executable comparison, and SHA-1 of every filesystem file.

| Supplied image | IP.BIN product/version | Region | Boot size | Boot SHA-1 |
|---|---|---|---:|---|
| Original CDI and newly supplied CDI | T-9715N V1.001 | JUE (modified header, ECHELON publisher) | 2024780 | `51384b12a9de6d9a44caecaadafd29dca86807cf` |
| USA CHD and USA CUE/BIN | T-9715N V1.001 | U, MIDWAY publisher | 2024780 | `5f49f40a0a30fe240d8868025201326b266e369b` |
| PAL GDI and Europe En/Fr/De/Es/It CUE/BIN | T9708D 50 V1.001 | E | 1609084 | `7e1288a58f26eedf76cc7bdbaeb53a72a9332fbc` |

USA header date: 2000-09-08. Europe header date: 2000-10-06. These are observed image headers,
not independent authentication against a disc preservation database.
The two CDIs have the same whole-image SHA-256:
`2e82dedea985fbc5cc7a33ae4cf3372e80f2f4a9a01c0d044b31d2746605426c`.
The USA CHD is the exact executable hash reported by the RecompLauncher user.

## USA: compatible instructions, different disc addresses

Only six executable bytes differ, at three 32-bit literal-pool locations. No instructions,
function boundaries, relocation contents, or enhancement-hook addresses moved.

| Guest literal address | CDI value | USA GD-ROM value | Meaning |
|---|---:|---:|---|
| `0x0C0F2DD0` | 150 | 45150 | Filesystem FAD boundary |
| `0x0C0F319C` | 150 | 45150 | Filesystem FAD boundary |
| `0x0C135EB8` | 11866 | 45166 | Primary volume descriptor FAD |

The translator previously embedded those CDI pool values in generated C++. Accepting the USA
hash alone would retain stale disc addresses. New `[binary].runtime_literals` marks just these
pool words for reads from guest RAM; all other folding remains unchanged. With that option,
translating the CDI executable and the USA executable produces identical sets of 16 C++/header
files, including every relocated unit. Generated outputs remain under `/tmp` or ignored build
folders, never in the repository.

New `[disc].compatible_sha1_1st_read` is an exact allowlist shared by config, launcher and runtime.
Only the verified USA hash is added. PAL and arbitrary modified hashes remain rejected.
The runtime now reads the executable from the selected disc even when a developer's extracted
copy exists; otherwise it could silently load the wrong revision's pool values.
Developer `dc.py setup` also accepts the allowlist and validates before writing an extraction.
Its TOML parser uses Python 3.11+ (documented for both source-build platforms).

The CDI has 220 files and the USA GD-ROM 222: 215 common files are identical; four differ
(`1ST_READ.BIN`, `AMDEMO.BIN`, `MANIFEST.BIN`, `WARNING.DA`). The CDI alone has padding
`000DUMMY.DAT`; the USA layout additionally has `CHEATS/GIRL.MDC1`,
`CHEATS/XC0P_DINOMOUTH.PVR`, and `CIN8_1.STR`. Every USA CHD filesystem file has the same name,
size and content hash in the USA CUE/BIN set; track container differences do not require another
executable profile.

### Snow cinematic audio

The USA GD-ROM includes native `CIN8_1.STR` (608256 bytes), missing from the CDI. The port's
six-second replacement helicopter cue is now restricted to the CDI. A startup extension selects
native audio when the verified executable's PVD literal is 45166. Synthetic cue tests verify
that the retail cue and native SFX mute are untouched while the CDI enhancement remains active.
The PAL image includes the same stream under `KLINK/`, but PAL is not enabled by this change.

## PAL: a loader and multiple game executables

The PAL top-level `1ST_READ.BIN` is not the USA game's main executable. Its filesystem includes
`GINRESET.BIN`, `GINXFER.BIN`, and separate game executables under `KLINK/`. The PAL GDI and
Europe CUE/BIN have identical filesystem names, sizes and hashes across all 728 files.

| Secondary executable | Size | SHA-1 |
|---|---:|---|
| `KLINK/KLINKENGLISH.BIN` | 2027660 | `c0b84ee7050731b7ed80fa54950703fe53a6a03c` |
| `KLINK/KLINKENGLISH60HZ.BIN` | 2027660 | `7e81e3336ab4d5dd6c7094f00e388ef4d9d12192` |
| `KLINK/KLINKUS.BIN` | 2024924 | `19a113111eab4ba43b23d5f7dbe3fb864b25088b` |

There are also French, German, Italian and Spanish 50/60 Hz executables. Their code/data layouts
are different from USA; even `KLINKUS.BIN` is not the supported USA executable. Many assets live
under `KLINK/`, and level/model archives differ. Removing the directory prefix yields 179 files
identical to USA and 33 changed common assets. This requires a separate translation/profile;
it cannot use the USA hash allowlist or just a container conversion.

A sensible first PAL target is English 60 Hz, followed by other language/video-mode profiles.
Required work: choose/verify the secondary executable, account for the original loader's
initialization/path setup, translate/discover relocations, map camera/menu/controller/HUD data
and hooks, and validate embedded level-code overlays and frame timing. Full language selection
needs profiles or overlay handling for the other executables.

Preliminary unique 24-byte instruction-prefix matches (candidates only, not validated mappings):
USA camera `0x0C0572A8` -> English60 `0x0C05B790`; logo status `0x0C12376E` ->
`0x0C12B00E`; cinematic updater `0x0C025820` -> `0x0C025900`; sound start
`0x0C01DCB4` -> `0x0C01DDE8`. Projection submit did not match that prefix. None of these
candidates are installed as PAL hooks. The PAL boot hash deliberately remains unsupported.

## Verification and local evidence

- `armymensh_disc_revision_tests`: strict hash/config parsing, unknown-image rejection, selective
  runtime pool reads with synthetic SH-4, and native-versus-added Snow cue handling.
- Actual launcher verification: CDI and USA CHD accepted; PAL GDI rejected.
- Same translation for CDI and USA: all 16 C++/header files match.
- `dc.py setup` accepts the USA CHD; removing its compatible hash causes rejection before any
  boot file is written.
- CDI and USA CHD: headless 600-frame boots, exit 0, 2494 sectors read, zero untranslated calls.
  RAM dumps confirm each image's own pool values despite an existing CDI developer extraction.
- PAL GDI: direct start exits 3 before guest execution, including with an extracted CDI present.
- Fourteen first-mission mouse/camera scenarios pass on USA CHD, including normal/aim motion,
  disabled/menu isolation, separate sensitivities, pitch clamp, horizontal-only mode and strafe.
- USA CUE/BIN: headless 600-frame boot, exit 0, zero untranslated calls. Europe CUE/BIN
  correctly exits 3 before execution.
- Rebuilt Windows frontend executable: Wine boots USA CHD and USA CUE for 600 frames, exit 0,
  zero untranslated calls; Europe CUE exits 3. Native Windows play testing remains pending.
- Validation build locations: `build-release/game/army-men-sarges-heroes-recomp` and
  `build-windows-fe/game/army-men-sarges-heroes-recomp.exe`. The ordinary development `build/`
  tree is separate. USA support is included in release v0.1.7; PAL remains unsupported.

Local-only full inventories, identity reports, generated comparisons, extracted comparison inputs,
test logs and RAM: `/tmp/army-disc-versions/`. Relevant test target:
`cmake --build <build> --target armymensh_disc_revision_tests`, then
`<build>/tools/armymensh_disc_revision_tests [CDI USA_CHD_OR_CUE [PAL_GDI_OR_CUE]]`.
