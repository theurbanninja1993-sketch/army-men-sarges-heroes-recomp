#!/usr/bin/env python3
"""Verify packaged PAL routing and all stock language/video choices using an owner's disc."""
import argparse
import concurrent.futures
import os
from pathlib import Path
import struct
import subprocess
import tempfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--exe', type=Path, required=True)
p.add_argument('--disc', type=Path, required=True)
p.add_argument('--out', type=Path)
p.add_argument('--jobs', type=int, default=2)
a = p.parse_args()
a.exe = a.exe.resolve()
a.disc = a.disc.resolve()
out = (a.out or Path(tempfile.mkdtemp(prefix='army-pal-startup-'))).resolve()
out.mkdir(parents=True, exist_ok=True)

def run(case):
    language, hz = case
    name = f'{language}-{hz}'
    # The selector is live by frame500. Right cycles five languages, down selects60.
    presses = [f'right@{520 + n*40}' for n in range(language)]
    if hz == 60:
        presses.append('down@700')
    presses += ['a@750', 'a@900', 'a@1050', 'a@1200']
    env = os.environ.copy()
    env['DREAM_DUMP_RAM'] = str(out / f'{name}.ram')
    cmd = [str(a.exe), '--no-launcher', '--disc', str(a.disc), '--max-frames', '2000',
           '--rtc-seed', '1000000', '--no-audio', '--settings', str(out/f'{name}.ini'),
           '--vmu', str(out/f'{name}-vmu.bin'), '--press', ','.join(presses)]
    with (out/f'{name}.log').open('w') as log:
        subprocess.run(cmd, env=env, stdout=log, stderr=subprocess.STDOUT,
                       check=True, timeout=240)
    log = (out/f'{name}.log').read_text()
    assert 'PAL preview: startup selection restricted to English / 60 Hz.' in log, name
    assert 'untranslated call targets: 0' in log, name
    ram = (out/f'{name}.ram').read_bytes()
    # The verified English60 entry target after the original loader handoff.
    assert struct.unpack_from('<I', ram, 0x1002c)[0] == 0x8c0f0b30, name
    return name

cases = [(language, hz) for language in range(5) for hz in (50, 60)]
with concurrent.futures.ThreadPoolExecutor(max_workers=max(1,a.jobs)) as pool:
    for name in pool.map(run, cases):
        print('PASS:', name, flush=True)
print(f'PASS: all ten startup choices load English60; evidence in {out}')
