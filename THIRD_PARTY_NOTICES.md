# Third-party notices

The port includes a modified Dreamcomp snapshot; its source is in `dreamcomp/`.
See [Dreamcomp's notices](dreamcomp/THIRD_PARTY_NOTICES.md) for upstream attribution.

| Component | License | Source / notice |
|---|---|---|
| Dreamcomp / dream-recomp | GPL-2.0 (upstream terms retained) | `dreamcomp/LICENSE`, `dreamcomp/engine/LICENSE` |
| Flycast portions via dream-recomp | GPL-2.0 | Engine source notices |
| libchdr and bundled codecs | BSD-3-Clause and component terms | `dreamcomp/engine/third_party/libchdr/` |
| toml++ | MIT | `dreamcomp/engine/third_party/tomlplusplus/LICENSE` |
| doctest (tests only) | MIT | `dreamcomp/engine/third_party/doctest/LICENSE.txt` |
| SDL 3.4.18 (Linux package) | zlib | https://github.com/libsdl-org/SDL/tree/release-3.4.18 |
| RmlUi 6.3 | MIT | https://github.com/mikke89/RmlUi/tree/6.3 |
| FreeType 2.14.3 | FTL or GPL-2.0 | https://github.com/freetype/freetype/tree/VER-2-14-3 |
| Inter 4.1 | SIL OFL 1.1 | `frontend/fonts/Inter-LICENSE.txt` in binary package |
| PromptFont 1.10 | SIL OFL 1.1 | `frontend/fonts/PromptFont-LICENSE.txt` in binary package |
| Vulkan loader / drivers | System-provided | Not bundled |

Binary archives include dependency license texts in `licenses/`. Their companion source
archive includes the port/framework source and the SDL, RmlUi, and FreeType source used to
build that release. The fonts and their licenses are included with the launcher resources.
