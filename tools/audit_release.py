#!/usr/bin/env python3
"""Reject game files, generated translations, secrets and binaries in Git source/history."""
from __future__ import annotations
import argparse
import re
import subprocess
from pathlib import PurePosixPath

BLOCKED = {'.gdi', '.cdi', '.cue', '.chd', '.iso', '.bin', '.raw', '.img', '.mds', '.mdf',
           '.nrg', '.pvr', '.pvm', '.afs', '.adx', '.ahx', '.sfd', '.str', '.kat', '.mlt',
           '.osb', '.dat', '.vmu', '.vms', '.vmi', '.dci', '.ta', '.vram', '.elf', '.exe',
           '.dll', '.so', '.a', '.o', '.obj', '.wav', '.mp4', '.webm', '.ppm', '.zip', '.7z'}
MAGIC = (b'SEGA SEGAKATANA', b'GBIX', b'PVRT', b'PVMH', b'AFS\0', b'CRID', b'\x7fELF',
         b'\x00\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00')
SECRET = re.compile(rb'(?:gh[pousr]_[A-Za-z0-9]{36,}|github_pat_[A-Za-z0-9_]{40,}|'
                    rb'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----)')

def git(*args):
    return subprocess.check_output(['git', *args])

def inspect(path, data):
    p = PurePosixPath(path)
    errors = []
    if p.suffix.lower() in BLOCKED:
        errors.append('blocked file type')
    if any(part.lower() in {'gen', 'generated', 'extracted', 'shots', 'traces', '.dreamcomp', '.deps'}
           or part in {'build', 'build-release', 'build-debug', 'build-linux', 'build-windows',
                       'build_release', 'build_debug'} or part.startswith('cmake-build-') for part in p.parts[:-1]):
        errors.append('generated/local directory')
    if p.name.lower().startswith('capture-'):
        errors.append('gameplay capture')
    if any(data.startswith(m) for m in MAGIC):
        errors.append('game/binary signature')
    if b'\0' in data:
        errors.append('binary content in source repository')
    if len(data) > 2 * 1024 * 1024:
        errors.append('unexpectedly large source file')
    if SECRET.search(data):
        errors.append('possible credential/private key')
    if re.search(rb'//.*Generated.*dream-translate', data[:1024], re.I):
        errors.append('generated game translation')
    return [f'{path}: {e}' for e in errors]

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--history', action='store_true')
    args = ap.parse_args()
    failures = []; count = 0; seen = set()
    # Index bytes, not working-tree bytes: this checks exactly what would be committed/pushed.
    for row in git('ls-files', '-s', '-z').split(b'\0'):
        if not row: continue
        meta, path = row.split(b'\t', 1)
        mode, oid, stage = meta.split()
        if mode == b'160000':
            failures.append(path.decode() + ': unresolved submodule (vendor reviewed source instead)')
            continue
        name = path.decode(); data = git('cat-file', 'blob', oid.decode())
        failures += inspect(name, data); seen.add((oid, name)); count += 1
    if args.history:
        for commit in git('rev-list', '--all').splitlines():
            for row in git('ls-tree', '-r', '-z', commit.decode()).split(b'\0'):
                if not row: continue
                meta, path = row.split(b'\t', 1); mode, kind, oid = meta.split()
                name = path.decode()
                if kind != b'blob' or (oid, name) in seen: continue
                seen.add((oid, name)); count += 1
                failures += inspect(name, git('cat-file', 'blob', oid.decode()))
    for failure in failures: print(failure)
    print(f'Checked {count} source blobs; {len(failures)} findings.')
    return bool(failures)
if __name__ == '__main__':
    raise SystemExit(main())
