# Vendored Dreamcomp source

Upstream: https://github.com/danielgomesvieira2000/dreamcomp
Base commit: 2f7b7c333e1600ba70c506b86ae1789edf8d5447

This directory includes the local Army Men engine/frontend changes used by this release,
including CDI support. Do not reapply the historical port patches to this snapshot.
Engine change notes are in docs/engine-changes.md and engine/docs/engine-changes.md.
libchdr is included as source rather than as a Git submodule, at commit 8e7b8bd32bc676b7e5c6b42fe7d2daca986c4a0d.

Disc files, generated game translation, build products, binary synthetic fixtures and Crazy Taxi
capture golden files are omitted. The omitted synthetic fixtures are only used by optional
upstream tests; the port's release build and port tests do not require them. Their original
assembly/source generators remain available.

Upstream licenses and attribution remain in LICENSE, engine/LICENSE and THIRD_PARTY_NOTICES.md.
