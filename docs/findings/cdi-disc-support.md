# Direct CDI support (2026-10-27)

The launcher file picker and runtime now accept DiscJuggler v2/v3/v3.5 CDI images directly.
No converted tracks or extracted boot executable are required by a binary release. Existing
boot SHA-1 revision validation remains mandatory. The Python developer setup/extraction tools
have not gained a CDI importer; this change covers the launcher and running prebuilt ports.

Implementation: engine/runtime/src/gdrom/cdi.cpp, registered by open_disc; frontend file filter.
Reads stored pregaps and session metadata with bounds checks, translates FAD to LBA, normalizes
Mode 1/Mode 2 data to 2048 bytes, preserves audio and drops optional subchannel bytes.
The source CDI is opened read-only. No game data is added to the repository.

Validation:
- Optional armymensh_disc_tests target: 54 synthetic layouts covering three versions, stored
  sector formats, audio/data, optional metadata, and reverse seeks; eight malformed inputs rejected.
- Original local CDI: all 317532 sectors match existing GDI track data byte for byte.
- Actual launcher verify_disc accepts the CDI and rejects an intentionally wrong revision hash.
- Game rebuilt. Fresh-profile CDI-only headless boot with a copied config and no extracted
  executable completed 600 guest frames, 2494 sectors read, exit 0, zero untranslated calls.
  Log: /tmp/army-cdi-smoke-q0kyyrib/run.log. Interactive file-picker/gameplay check remains manual.

Build tests with `cmake --build build --target armymensh_disc_tests`; run
`build/tools/armymensh_disc_tests [CDI GDI]` from the port. Images are optional, local-only inputs.

Engine changes preserved in patches/dreamcomp-cdi-disc.patch. This patch includes only CDI
changes and can follow the existing port engine patches.
