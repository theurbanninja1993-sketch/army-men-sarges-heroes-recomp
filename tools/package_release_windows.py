#!/usr/bin/env python3
"""Stage the Windows x64 release from a cross-build tree, audit it, and make the zip.

The Windows build is produced by clang-cl + lld-link with the MSVC CRT/STL and Windows SDK from
xwin (see HANDOFF.md / BUILDING.md). This script only packages an existing build tree; it never
touches game data.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import zipfile
from pathlib import Path

EXE = "army-men-sarges-heroes-recomp.exe"
DLL = "SDL3.dll"
BLOCKED = {".cdi", ".gdi", ".cue", ".chd", ".iso", ".bin", ".raw", ".img", ".mds", ".mdf",
           ".nrg", ".pvr", ".pvm", ".afs", ".adx", ".sfx", ".str", ".kat", ".mlt", ".osb",
           ".dat", ".vmu", ".vms", ".vmi", ".dci", ".ta", ".vram", ".wav", ".mp4", ".webm",
           ".ppm", ".zip", ".7z"}


def copy(src: Path, dst: Path) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--build", type=Path, default=Path("build-windows-fe"))
    ap.add_argument("--sdl-dir", type=Path, default=Path("/tmp/opencode/sdl3-win/SDL3-3.4.18"))
    ap.add_argument("--linux-stage", type=Path, default=None,
                    help="a Linux release-stage folder to take license texts from")
    ap.add_argument("--version", default="0.1.4")
    a = ap.parse_args()
    root = Path(__file__).resolve().parent.parent
    subprocess.run(["git", "-C", str(root), "diff", "--exit-code", "HEAD"], check=True)
    build = a.build.resolve()
    stage = root / "release-stage"
    dist = root / "dist"
    stage.mkdir(exist_ok=True)
    dist.mkdir(exist_ok=True)
    tree = stage / f"army-men-sarges-heroes-recomp-v{a.version}-Windows-x64"
    if tree.exists():
        raise RuntimeError("stage already exists; choose an empty release-stage")
    tree.mkdir()

    copy(build / "game" / EXE, tree / EXE)
    copy(build / "game" / DLL, tree / DLL)
    shutil.copytree(build / "game/frontend", tree / "frontend")
    copy(root / "game/armymensh.toml", tree / "armymensh.toml")
    copy(root / "game/hud_overrides.ini", tree / "hud_overrides.ini")
    copy(root / "recomplauncher.json", tree / "recomplauncher.json")
    for name in ["README.md", "BUILDING.md", "LICENSE", "LICENSE-NOTICE.md", "THIRD_PARTY_NOTICES.md"]:
        copy(root / name, tree / name)

    # Licenses: the same set the Linux package carries.
    if a.linux_stage:
        lic = a.linux_stage / "licenses"
        if lic.is_dir():
            shutil.copytree(lic, tree / "licenses")
    if not (tree / "licenses").is_dir():
        # Fall back to the checked-in texts and the Windows build's fetched sources.
        lic = tree / "licenses"
        lic.mkdir()
        copy(root / "dreamcomp/LICENSE", lic / "Dreamcomp-LICENSE.txt")
        copy(root / "dreamcomp/engine/LICENSE", lic / "dream-recomp-LICENSE.txt")
        copy(root / "dreamcomp/engine/third_party/libchdr/LICENSE.txt", lic / "libchdr-LICENSE.txt")
        copy(root / "dreamcomp/engine/third_party/tomlplusplus/LICENSE", lic / "tomlplusplus-LICENSE.txt")
        copy(a.sdl_dir / "LICENSE.txt", lic / "SDL-LICENSE.txt")
        deps = build / "_deps"
        for src, name in [
            (deps / "dreamcomp_rmlui-src/LICENSE.txt", "RmlUi-LICENSE.txt"),
            (deps / "dreamcomp_freetype-src/LICENSE.TXT", "FreeType-LICENSE.txt"),
            (deps / "dreamcomp_freetype-src/docs/FTL.TXT", "FreeType-FTL.txt"),
            (deps / "dreamcomp_freetype-src/docs/GPLv2.TXT", "FreeType-GPLv2.txt"),
        ]:
            if src.is_file():
                copy(src, lic / name)
        for p in (root / "dreamcomp/engine/third_party/libchdr/deps").rglob("*"):
            if p.is_file() and any(x in p.name.lower() for x in ["license", "copying"]):
                rel = p.relative_to(root / "dreamcomp/engine/third_party/libchdr")
                copy(p, lic / "libchdr" / rel)

    commit = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
    meta = {
        "version": a.version,
        "platform": "Windows x64",
        "source_commit": commit,
        "build": "cross-compiled on Linux with clang-cl + lld-link, MSVC CRT/STL and Windows SDK from xwin",
        "runtime": "MSVC C/C++ runtime linked statically; SDL3 bundled; Vulkan from the GPU driver",
        "game_assets_included": False,
        "compiled_static_translation_in_executable": True,
    }
    (tree / "BUILD-INFO.json").write_text(json.dumps(meta, indent=2) + "\n")

    # Explicit allow-list: nothing but the release layout may enter the zip.
    fonts = {"Inter-Regular.ttf", "Inter-SemiBold.ttf", "Inter-Bold.ttf", "promptfont.ttf",
             "Inter-LICENSE.txt", "PromptFont-LICENSE.txt"}
    root_files = {EXE, DLL, "armymensh.toml", "hud_overrides.ini", "recomplauncher.json",
                  "README.md", "BUILDING.md", "LICENSE", "LICENSE-NOTICE.md",
                  "THIRD_PARTY_NOTICES.md", "BUILD-INFO.json", "MANIFEST.sha256"}
    for p in tree.rglob("*"):
        if not p.is_file():
            continue
        rel = p.relative_to(tree).as_posix()
        if p.suffix.lower() in BLOCKED:
            raise RuntimeError("blocked file type: " + rel)
        allowed = rel in root_files or rel.startswith("licenses/")
        allowed |= rel in {"frontend/launcher.rml", "frontend/dreamcomp.rcss"}
        allowed |= rel.startswith("frontend/fonts/") and p.name in fonts
        if not allowed:
            raise RuntimeError("unexpected package file: " + rel)
        if p.name.lower().startswith("capture-"):
            raise RuntimeError("gameplay capture: " + rel)

    lines = []
    for p in sorted(tree.rglob("*")):
        if p.is_file():
            lines.append(sha256(p) + "  " + p.relative_to(tree).as_posix())
    (tree / "MANIFEST.sha256").write_text("\n".join(lines) + "\n")

    out = dist / (tree.name + ".zip")
    if out.exists():
        out.unlink()
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for p in sorted(tree.rglob("*")):
            if p.is_file():
                z.write(p, tree.name + "/" + p.relative_to(tree).as_posix())
    print("Archive content audit: passed (explicit file list; no game data)")
    print(out)
    digest = sha256(out)
    print(digest + "  " + out.name)
    sums = dist / "SHA256SUMS"
    lines = []
    if sums.is_file():
        lines = [l for l in sums.read_text().splitlines() if l.strip() and not l.endswith("  " + out.name)]
    lines.append(digest + "  " + out.name)
    sums.write_text("\n".join(sorted(lines)) + "\n")


if __name__ == "__main__":
    main()
