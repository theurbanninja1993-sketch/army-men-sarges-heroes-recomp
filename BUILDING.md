# Building

The repository includes the modified Dreamcomp source in `dreamcomp/`. No game image, extracted
boot executable, or generated translation is provided. Do not apply `patches/` again: those
changes are already present in the included snapshot.

## Linux

Install CMake 3.24+, Ninja, a C++20 compiler, Python 3.11+, SDL3 development files, Vulkan headers
and loader, and `glslc`. Git and network access are needed for frontend dependencies/fonts on
first configure. Upstream engine tests are disabled for the port build; some optional upstream
binary fixtures have deliberately been omitted from this repository.

From the repository root, with your own matching GDI/CUE/CHD image:

```sh
python3 dreamcomp/tools/dc.py setup . --disc /path/to/your-game.gdi
python3 dreamcomp/tools/dc.py build .
./build/game/army-men-sarges-heroes-recomp --launcher
```

The launcher/runtime supports CDI directly. The Python developer extraction tool currently
requires GDI, CUE, or CHD; convert a CDI locally for a source build. Never upload converted tracks.

The supported boot revision and SHA-1 are listed in README.md. Frontend dependencies are pinned
in `dreamcomp/cmake/DreamcompFrontend.cmake`. A source release includes copies under
`third_party_sources/`; you may use CMake `FETCHCONTENT_SOURCE_DIR_DREAMCOMP_FREETYPE` and
`FETCHCONTENT_SOURCE_DIR_DREAMCOMP_RMLUI` to point to those copies.

## Release build

`tools/release/Dockerfile` defines an Ubuntu 24.04 toolchain with SDL 3.4.18. The release uses
Clang, Release mode, and the default interpreter fallback (matching the tested development
build). Game extraction/translation happens locally; none of its output belongs in a source
archive. Release packages contain the compiled executable but no standalone game files.

Build the toolchain image with `docker build -t armymensh-release tools/release` and use it to
build a separate `build-release/` tree with `DREAMCOMP_DIR` pointing to the included snapshot.
`tools/package_release.py` stages archives from an explicit file list and audits their contents.

## Windows

Windows version coming soon. A Windows build has not yet been validated for this port.
