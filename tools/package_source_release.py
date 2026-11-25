#!/usr/bin/env python3
"""Package matching project source and reviewed dependency sources without a Linux binary."""
from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
from pathlib import Path

from audit_release import inspect
from package_release import archive, copy

DEPENDENCIES = {"sdl": "3.4.18", "rmlui": "6.3", "freetype": "2.14.3"}
FONT_FILES = {"Inter-Regular.ttf", "Inter-SemiBold.ttf", "Inter-Bold.ttf", "promptfont.ttf",
              "Inter-LICENSE.txt", "PromptFont-LICENSE.txt"}


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--version", required=True)
    ap.add_argument("--dependency-source-stage", type=Path, required=True,
                    help="reviewed source stage containing third_party_sources and BUILD-INFO.json")
    ap.add_argument("--build", type=Path, default=Path("build-windows-fe"))
    args = ap.parse_args()
    root = Path(__file__).resolve().parent.parent
    subprocess.run(["git", "-C", str(root), "diff", "--exit-code", "HEAD"], check=True)
    donor = args.dependency_source_stage.resolve()
    donor_meta = json.loads((donor / "BUILD-INFO.json").read_text())
    for name, version in DEPENDENCIES.items():
        if donor_meta.get(name) != version:
            raise RuntimeError(f"dependency source mismatch: {name} must be {version}")
    stage = root / "release-stage" / f"army-men-sarges-heroes-recomp-v{args.version}-source"
    output = root / "dist" / (stage.name + ".tar.gz")
    if stage.exists() or output.exists():
        raise RuntimeError("source stage/archive already exists; select a new version")
    stage.mkdir(parents=True)
    output.parent.mkdir(exist_ok=True)
    names = subprocess.check_output(["git", "-C", str(root), "ls-files", "-z"]).decode().split("\0")
    for name in filter(None, names):
        data = (root / name).read_bytes()
        errors = inspect(name, data)
        if errors:
            raise RuntimeError("\n".join(errors))
        copy(root / name, stage / name)
    # Reuse complete, previously reviewed sources, never the donor project's code or binaries.
    for name in ("SDL", "RmlUi", "FreeType"):
        src = donor / "third_party_sources" / name
        if not src.is_dir() or not (src / "CMakeLists.txt").is_file():
            raise RuntimeError(f"missing dependency source: {src}")
        shutil.copytree(src, stage / "third_party_sources" / name)
    fonts = args.build.resolve() / "game/frontend/fonts"
    for name in FONT_FILES:
        copy(fonts / name, stage / "frontend_fonts" / name)
    commit = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
    metadata = {"version": args.version, "source_commit": commit, "platform": "Source",
                **DEPENDENCIES, "pal_english_60hz_preview": True,
                "game_assets_included": False,
                "compiled_static_translation_in_executable": False}
    (stage / "BUILD-INFO.json").write_text(json.dumps(metadata, indent=2) + "\n")
    count = 0
    for path in stage.rglob("*"):
        if not path.is_file():
            continue
        relative = path.relative_to(stage).as_posix()
        if (path.parent == stage / "frontend_fonts" and path.name in FONT_FILES
                and path.suffix == ".ttf"):
            if path.read_bytes() != (fonts / path.name).read_bytes():
                raise RuntimeError("font differs from Windows build: " + relative)
        else:
            errors = inspect(relative, path.read_bytes())
            if errors:
                raise RuntimeError("\n".join(errors))
        count += 1
    manifest = [hashlib.sha256(p.read_bytes()).hexdigest() + "  " + p.relative_to(stage).as_posix()
                for p in sorted(stage.rglob("*")) if p.is_file()]
    (stage / "MANIFEST.sha256").write_text("\n".join(manifest) + "\n")
    archive(stage, output)
    print(f"Source archive audit: {count} files checked; no game data/generated translation")
    print(output)
    print(hashlib.sha256(output.read_bytes()).hexdigest() + "  " + output.name)


if __name__ == "__main__":
    main()
