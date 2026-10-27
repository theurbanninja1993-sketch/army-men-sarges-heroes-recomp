#!/usr/bin/env python3
"""Stage release files explicitly; never copy a build directory or local disc data wholesale."""
from __future__ import annotations
import argparse
import hashlib
import json
import shutil
import subprocess
import tarfile
from pathlib import Path
from audit_release import inspect

EXE = 'army-men-sarges-heroes-recomp'

def copy(src, dst):
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)

def source_tree(src, dst):
    # Only versioned upstream source, without .git, build outputs or binary examples/fixtures.
    names = subprocess.check_output(['git', '-c', f'safe.directory={src}', '-C', str(src),
                                     'ls-files', '-z']).decode().split('\0')
    count = 0
    for name in names:
        if not name or name.startswith('.github/'): continue
        p = src / name
        if not p.is_file(): continue
        data = p.read_bytes()
        errors = inspect(name, data)
        if errors:
            if p.suffix.lower() in {'.c', '.h', '.cpp', '.hpp', '.cmake', '.in'}:
                raise RuntimeError('required dependency source rejected: ' + '\n'.join(errors))
            continue
        copy(p, dst / name); count += 1
    if count < 20: raise RuntimeError(f'incomplete dependency source: {src}')

def archive(tree, output):
    with tarfile.open(output, 'w:gz') as tar:
        tar.add(tree, arcname=tree.name)

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--build', type=Path, default=Path('build-release'))
    ap.add_argument('--sdl-prefix', type=Path, default=Path('/opt/sdl3'))
    ap.add_argument('--sdl-source', type=Path, default=Path('/opt/SDL'))
    ap.add_argument('--version', default='0.1.0')
    a = ap.parse_args()
    root = Path(__file__).resolve().parent.parent
    subprocess.run(['git', '-C', str(root), 'diff', '--exit-code', 'HEAD'], check=True)
    stage = root / 'release-stage'; dist = root / 'dist'
    stage.mkdir(exist_ok=True); dist.mkdir(exist_ok=True)
    binary = stage / f'{EXE}-v{a.version}-linux-x86_64'
    source = stage / f'{EXE}-v{a.version}-source'
    if binary.exists() or source.exists(): raise RuntimeError('stage already exists; choose an empty release-stage')
    binary.mkdir(); source.mkdir()
    build = a.build.resolve()
    copy(build / 'game' / EXE, binary / EXE)
    copy(root / 'game/armymensh.toml', binary / 'armymensh.toml')
    copy(root / 'game/hud_overrides.ini', binary / 'hud_overrides.ini')
    shutil.copytree(build / 'game/frontend', binary / 'frontend')
    copy((a.sdl_prefix / 'lib/libSDL3.so.0').resolve(), binary / 'lib/libSDL3.so.0')
    subprocess.run(['patchelf', '--set-rpath', '$ORIGIN/lib', str(binary / EXE)], check=True)
    for name in ['README.md', 'BUILDING.md', 'LICENSE', 'LICENSE-NOTICE.md', 'THIRD_PARTY_NOTICES.md']:
        copy(root / name, binary / name)
    license_files = {
        'SDL-LICENSE.txt': a.sdl_source / 'LICENSE.txt',
        'RmlUi-LICENSE.txt': build / '_deps/dreamcomp_rmlui-src/LICENSE.txt',
        'FreeType-LICENSE.txt': build / '_deps/dreamcomp_freetype-src/LICENSE.TXT',
        'FreeType-FTL.txt': build / '_deps/dreamcomp_freetype-src/docs/FTL.TXT',
        'FreeType-GPLv2.txt': build / '_deps/dreamcomp_freetype-src/docs/GPLv2.TXT',
        'Dreamcomp-LICENSE.txt': root / 'dreamcomp/LICENSE',
        'dream-recomp-LICENSE.txt': root / 'dreamcomp/engine/LICENSE',
        'libchdr-LICENSE.txt': root / 'dreamcomp/engine/third_party/libchdr/LICENSE.txt',
        'tomlplusplus-LICENSE.txt': root / 'dreamcomp/engine/third_party/tomlplusplus/LICENSE',
    }
    # libchdr's individual codecs retain separate copyright/license files.
    for p in (root / 'dreamcomp/engine/third_party/libchdr/deps').rglob('*'):
        if p.is_file() and any(x in p.name.lower() for x in ['license', 'copying']):
            rel = p.relative_to(root / 'dreamcomp/engine/third_party/libchdr')
            copy(p, binary / 'licenses/libchdr' / rel)
    for name, path in license_files.items(): copy(path, binary / 'licenses' / name)
    # The index is the reviewed source manifest; ignored local files cannot enter the archive.
    names = subprocess.check_output(['git', '-C', str(root), 'ls-files', '-z']).decode().split('\0')
    for name in names:
        if not name: continue
        data = (root / name).read_bytes()
        errors = inspect(name, data)
        if errors: raise RuntimeError('\n'.join(errors))
        copy(root / name, source / name)
    for name, path in [('SDL', a.sdl_source),
                       ('RmlUi', build / '_deps/dreamcomp_rmlui-src'),
                       ('FreeType', build / '_deps/dreamcomp_freetype-src')]:
        source_tree(path.resolve(), source / 'third_party_sources' / name)
    shutil.copytree(binary / 'frontend/fonts', source / 'frontend_fonts')
    # Reject surprises before making either uploadable archive.
    root_files = {EXE, 'armymensh.toml', 'hud_overrides.ini', 'README.md', 'BUILDING.md',
                  'LICENSE', 'LICENSE-NOTICE.md', 'THIRD_PARTY_NOTICES.md'}
    fonts = {'Inter-Regular.ttf', 'Inter-SemiBold.ttf', 'Inter-Bold.ttf', 'promptfont.ttf',
             'Inter-LICENSE.txt', 'PromptFont-LICENSE.txt'}
    for p in binary.rglob('*'):
        if not p.is_file(): continue
        rel = p.relative_to(binary).as_posix()
        allowed = rel in root_files or rel == 'lib/libSDL3.so.0' or rel.startswith('licenses/')
        allowed |= rel in {'frontend/launcher.rml', 'frontend/dreamcomp.rcss'}
        allowed |= rel.startswith('frontend/fonts/') and p.name in fonts
        if not allowed: raise RuntimeError('unexpected package file: ' + rel)
        if rel not in {EXE, 'lib/libSDL3.so.0'} and p.suffix != '.ttf':
            if b'\0' in p.read_bytes(): raise RuntimeError('unexpected binary: ' + rel)
    for p in source.rglob('*'):
        if not p.is_file() or p.parent.name == 'frontend_fonts' and p.name in fonts: continue
        errors = inspect(p.relative_to(source).as_posix(), p.read_bytes())
        if errors: raise RuntimeError('\n'.join(errors))
    print('Archive content audit: passed (explicit binary file list; source contains no game data)')
    commit = subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip()
    metadata = {'version': a.version, 'source_commit': commit,
                'platform': 'Linux x86_64', 'build_base': 'Ubuntu 24.04',
                'sdl': '3.4.18', 'rmlui': '6.3', 'freetype': '2.14.3',
                'game_assets_included': False, 'compiled_static_translation_in_executable': True}
    for tree in [binary, source]:
        (tree / 'BUILD-INFO.json').write_text(json.dumps(metadata, indent=2) + '\n')
        lines = [hashlib.sha256(p.read_bytes()).hexdigest() + '  ' + p.relative_to(tree).as_posix()
                 for p in sorted(tree.rglob('*')) if p.is_file()]
        (tree / 'MANIFEST.sha256').write_text('\n'.join(lines) + '\n')
        archive(tree, dist / (tree.name + '.tar.gz'))
    outputs = sorted(dist.glob(f'{EXE}-v{a.version}-*.tar.gz'))
    (dist / 'SHA256SUMS').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest() +
                                         '  ' + p.name + '\n' for p in outputs))
    print('\n'.join(str(p) for p in outputs))

if __name__ == '__main__': main()
