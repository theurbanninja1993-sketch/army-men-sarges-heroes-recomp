#!/usr/bin/env python3
"""Reproduce the first-mission mouse camera checks using the owner's disc (no GUI)."""
import argparse
import concurrent.futures
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PRESS = ("start@2200,start@2300,start@2400,start@2500,start@2600,b@2800,"
         "down@3100,a@3300,a@3500,start@3700,a@3900,a@7800")
CASES = {
    "base": (None, False, []),
    "turn": ("8400:250:125", False, []),
    "disabled": ("8400:250:125", False, ["--set", "mouse_look=false"]),
    "menu": ("2000:250:125", False, []),
    "aimbase": (None, True, []),
    "aimturn": ("8400:250:125", True, []),
    "aiminvert": ("8400:250:125", True,
                  ["--set", "mouse_invert_y=true", "--set", "mouse_sensitivity=0.24"]),
    "separate_normal": ("8400:250:125", False,
                        ["--set", "mouse_sensitivity=0.12", "--set", "mouse_aim_sensitivity=0.06"]),
    "aimseparate": ("8400:250:125", True,
                    ["--set", "mouse_sensitivity=0.24", "--set", "mouse_aim_sensitivity=0.06"]),
    "aimclamp": ("8400:4000:4000", True, []),
    "horizontal": ("8400:250:125", False, ["--set", "mouse_vertical=false"]),
    "aimhorizontal": ("8400:250:125", True, ["--set", "mouse_vertical=false"]),
    "strafeleft": (None, False, []),
    "straferight": (None, False, []),
}
STRAFE_PAD = {
    "strafeleft": "8400:8500:0:0:128:255",
    "straferight": "8400:8500:0:255:128:255",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--disc", type=Path, required=True)
    parser.add_argument("--exe", type=Path, default=ROOT / "build/game/army-men-sarges-heroes-recomp")
    parser.add_argument("--out", type=Path)
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    out = (args.out or Path(tempfile.mkdtemp(prefix="army-mouse-check-"))).resolve()
    out.mkdir(parents=True, exist_ok=True)

    def run(name):
        sample, aim, extra = CASES[name]
        env = os.environ.copy()
        for key in ("ARMY_TEST_MOUSE", "DREAM_EXP_PAD", "DREAM_EXP_ROT",
                    "DREAM_WATCH_READ", "DREAM_WATCH_WRITE"):
            env.pop(key, None)
        env["DREAM_DUMP_RAM"] = str(out / f"{name}.ram")
        if sample:
            env["ARMY_TEST_MOUSE"] = sample
        if aim:
            env["DREAM_EXP_PAD"] = "8200:8500:255:128:128"
        if name in STRAFE_PAD:
            env["DREAM_EXP_PAD"] = STRAFE_PAD[name]
        command = [str(args.exe.resolve()), "--config", "game/armymensh.toml", "--disc",
                   str(args.disc.resolve()), "--max-frames", "8450", "--rtc-seed", "1000000",
                   "--no-audio", "--settings", str(out / f"{name}-settings.ini"),
                   "--vmu", str(out / f"{name}-vmu.bin"),
                   "--press", PRESS] + extra
        with (out / f"{name}.log").open("w") as log:
            subprocess.run(command, cwd=ROOT, env=env, stdout=log,
                           stderr=subprocess.STDOUT, check=True, timeout=300)
        log = (out / f"{name}.log").read_text()
        assert "untranslated call targets: 0\n" in log, name
        assert "unmapped memory accesses: 1\n" in log and "0x8bfff1c0 w4 x1" in log, name
        ram = (out / f"{name}.ram").read_bytes()
        assert len(ram) == 16 * 1024 * 1024, name
        f32 = lambda address: struct.unpack_from("<f", ram, address)[0]
        result = {"pitch": f32(0x1A101C), "yaw": f32(0x1A1020),
                  "player_yaw": f32(0x411820),
                  "player_xz": [f32(0x411810), f32(0x411818)]}
        return name, result

    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        results = dict(pool.map(run, CASES))
    baseline = (out / "base.ram").read_bytes()
    for name in ("disabled", "menu"):
        assert (out / f"{name}.ram").read_bytes() == baseline, name
    expected = {"turn": (30, 25), "aimturn": (30, 15),
                "aiminvert": (60, -30), "aimclamp": (120, 75),
                "horizontal": (30, 10), "aimhorizontal": (30, 0), "separate_normal": (30, 25), "aimseparate": (15, 7.5)}
    for name, (yaw_step, pitch) in expected.items():
        result = results[name]
        reference = results["aimbase" if name.startswith("aim") else "base"]
        assert abs(math.remainder(result["yaw"] - reference["yaw"] - yaw_step, 360)) < 0.01, name
        assert abs(result["pitch"] - pitch) < 0.02, name
        assert abs(math.remainder(result["yaw"] - result["player_yaw"], 360)) < 0.01, name
        assert result["player_xz"] == reference["player_xz"], name
    for name in STRAFE_PAD:
        result = results[name]
        assert abs(math.remainder(result["player_yaw"] - results["base"]["player_yaw"], 360)) < 0.01, name
        assert result["player_xz"] != results["base"]["player_xz"], name
    origin = results["base"]["player_xz"]
    left = [v - o for v, o in zip(results["strafeleft"]["player_xz"], origin)]
    right = [v - o for v, o in zip(results["straferight"]["player_xz"], origin)]
    assert sum(a * b for a, b in zip(left, right)) < 0, "strafe directions must oppose"
    (out / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print(f"PASS: {len(CASES)} first-mission scenarios; results and logs in {out}")


if __name__ == "__main__":
    main()
