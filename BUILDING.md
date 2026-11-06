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

The supported boot revisions and SHA-1 allowlist are listed in README.md. Both verified USA
executables can be used for a source build; the three disc-address literals remain runtime reads. Frontend dependencies are pinned
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

Windows 10/11 x64. Install Visual Studio 2022 Build Tools with the C++ workload, LLVM
(`clang-cl`), CMake, Ninja, Python 3.11+, and the Vulkan SDK. `dreamcomp/tools/dc.py` finds the
toolchain and fetches SDL3 into `.deps/`.

From the repository root, in a Developer Command Prompt, with your own matching GDI/CUE/CHD image:

```bat
python dreamcomp\tools\dc.py doctor
python dreamcomp\tools\dc.py deps
python dreamcomp\tools\dc.py setup . --disc "D:\discs\your-game.gdi"
python dreamcomp\tools\dc.py build .
build\game\army-men-sarges-heroes-recomp.exe --launcher
```

The Python developer extraction tool does not take CDI; use GDI, CUE, or CHD, or copy the
extracted `1ST_READ.BIN` from a setup done on another machine. The runtime itself reads CDI.

The published Windows release is cross-compiled on Linux with clang-cl, lld-link, and the MSVC
CRT/STL and Windows SDK from `xwin` (see `tools/package_release_windows.py`). Native Windows
builds use the same generated translation; both paths produce the same game code.


## PAL English60 preview (Linux)

Extract the verified owner-supplied PAL disc locally with `armymensh_pal_extract DISC game/extracted/pal`.
This tool validates all three input hashes before writing any files. The extracted files and
translated output must never enter Git or a source archive.

Configure with `-DARMYMENSH_PAL_SUPPORT=ON -DDREAM_DEV_INTERPRETER=OFF`, then build
`armymensh_boot` and `armymensh_pal_controls`. The default executable detects PAL discs and
starts the PAL runtime beside it. Both executables need the same frontend assets and SDL library.
Use `tools/package_release.py --pal --version 0.1.8-pal-preview.1` in the release container;
the package allowlist includes only the extra runtime, its bootstrap config and PAL HUD rules.
The source archive also contains the translation-input configs, never their binary inputs.

The PAL preview constrains startup to English/60 Hz; this does not implement the other
languages or 50 Hz. `ARMYMENSH_PAL_PROBE` and `DREAM_DEV_INTERPRETER=ON` remain opt-in developer
investigation tools and must not be used for the published release build.
