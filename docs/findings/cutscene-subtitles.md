# Opening cinematic subtitles: USA versus PAL English

Investigated2026-11-08 using the user's native Windows test10 capture and local
owner-supplied disc images. No subtitle renderer change was made.

## Observations

The capture shows Sarge kneeling by the river/dock during the opening cinematic
(guest4438, presented4421). Replay inspection finds no visible flat font quads;
the on-screen flat pieces are the background and a bottom edge strip. This is not
an observation that subtitles were submitted and subsequently hidden by HUD layout.

The USA disc's CINEMATIC01.UFS contains the **exact complete ELF object** stored as
KLINK/US/CIN1C0.O on the PAL disc, at byte offset2203636, length53504. The full object
bytes match, not just its name or a few strings. Comparing it with PAL's
KLINK/ENGLISH/CIN1C0.O (61920bytes):

| ELF field | USA / PAL US asset | PAL localized English asset |
| --- | --- | --- |
| CnmHeader subtitle count at data+0x30 | 0 | 84 |
| CnmHeader subtitle table addend at data+0x34 | 0 | 0xa4a4 |
| _CnmText_Cinematic_Level_Cnm00 symbol | COMMON,64bytes | .data+0xa4a4,8064bytes |

The localized object contains dialogue strings absent from the USA object. Together
with the zero subtitle count and exact USA archive match, this establishes that
this opening cinematic has no subtitle cues in the original USA asset. It is not
a missing translation or a widescreen rendering regression. The user subsequently
confirmed remembering that the original NTSC version had no subtitles.

Hashes (SHA256):

- USA/PAL-US CIN1C0.O: c055138a7a2bc289337d6db41e35f81d8a72fcbd6d3f6bfeeb0dc120669a7765
- PAL English CIN1C0.O: 2f63efafa09649afcd00b1d63ef06cd89619f26f4da5cd1bab7bddcb5a1abdda
- USA CINEMATIC01.UFS: 8b14c5c61e3f286e17267d357372ecc6a2fde16ed30365d4ac28b8b3f8252b6c

Inspection used the project ISO9660 reader, readelf symbol/section/relocation tables,
and struct-based header reads. REA inspect_binary_layout was attempted but its
pwntools provider was unavailable (REA_PWNTOOLS_PYTHON unset); no provider setup or
configuration changes were made. No decompilation or runtime subtitle visibility
claim is attributed to that unavailable provider. Game objects/captures remain
local and are excluded from source and packages.

## Remaining PAL issue

The user observed occasional PAL subtitle-box jumps/flicker, including with the
experimental dispatcher disabled; it did not occur every run. That issue remains
unresolved and needs a PAL good/bad full capture. This NTSC capture cannot establish
its cause. Do not add USA subtitles or change subtitle rendering based on this capture.
