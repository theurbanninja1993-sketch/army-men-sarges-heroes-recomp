# Windows intermittent slowdown investigation

Updated 2026-11-25. Start here when continuing on the owner's Windows laptop.

## Current objective

Investigate intermittent severe slow frames during normal gameplay/cutscenes,
seen on the owner's Windows laptop during levels 1–3. Collect CPU performance
telemetry alongside the existing diagnostic profiles. No fix for these remaining
bursts has been identified. Do not publish a release or change power/driver/system
settings as part of baseline measurement.

The owner will play the affected sections. The Windows agent should handle local
commands, start/stop telemetry, preserve logs and analyze them. Work in native
Windows/PowerShell, not WSL/Wine, for this reproduction. Ask the owner for the
local extracted game folder if it cannot be found from the provided project.
Do not assume Linux filesystem paths exist on Windows.

## Repository versus runnable game

This repository contains source and documentation, not game data or generated
translations. Cloning it alone does not produce a playable executable. The owner
already has a local extracted diagnostic package named
`army-men-sarges-heroes-recomp-windows-frame-diagnostics-2`, copied via USB. Use
that build initially; do not substitute the published preview executable.

The ZIP SHA-256 is:
`50dfceac7a406d147432027d93ba83338ad12f414e0290f7d05ccdf03b7e4406`

Read the package's `BUILD-INFO.json` and `DIAGNOSTICS.txt` to verify it is local
frame-diagnostics-2. It is based on source commit
`e86d27f6caf8a69b229c4267f74189d25fe647e3` plus the timing instrumentation now
included in this repository. The diagnostic executables may still show the base
preview version. There is no new GitHub release for the diagnostic build.

Build instructions are in BUILDING.md. Static translation requires an owner's
supported disc image. Preserve existing settings and VMU saves; use separate
paths for automated tests. Do not upload discs, extracted assets, translations,
RAM/VRAM dumps, captures, saves, or raw user logs to GitHub. Keep diagnostics in a
separate local directory outside the repository. Log paths can expose usernames.

## Established evidence

Owner laptop GPU: NVIDIA RTX 3050 Laptop GPU. Windows native runs use Standard
latency (Vulkan swapchain), a 60 Hz screen, interpolation off, local tail dispatch
on. At 4x the game renders at 3413x1920; 2x is 1707x960; 1x is 853x480.

- Initial 4x Bathroom/level 3: 92.444 guest seconds / 125.36 host seconds;
  p99 136.24 ms, max 210.3 ms, 363 frames >33.4 ms.
- 1x: 105.506 / 105.53 seconds, p99 18.25 ms, max 24.8 ms, no frames >33.4 ms.
- 2x: 174.502 / 174.59 seconds, p99 29.63 ms, max 45.3 ms, five frames >33.4 ms;
  owner noticed no slowdown.
- Subsequent diagnostic-1 4x run was smooth: 152.832 / 153.06 seconds, p99
  18.23 ms, one isolated 153 ms hitch. Resolution alone is not a reliable trigger.
- Longer diagnostic-1 levels 1–3: 806.028 / 830.68 seconds, p99 18.46 ms,
  max 255.5 ms, 271 frames >33.4 ms. Most retained worst frames have substantial
  scheduled game-thread CPU time and negligible fence waits. A 256 ms frame had
  234.4 ms CPU and about 0.2 ms fence waits, with no texture decode/pipeline creation.
- Diagnostic-2: 445.916 / 449.76 seconds, p99 19.29 ms, max 161.5 ms,
  58 frames >33.4 ms. 43,079 total stack samples; 241 late-portion samples.
  Late samples span guest logic, ARM7 audio emulation, rendering preparation,
  memcpy and fog maths. No single leaf function dominates. 196/241 late leaf
  samples are in the executable, only two have the previously seen NVIDIA wait
  chain. CPU work is implicated, but extra work versus reduced execution speed
  is not distinguished. Do not claim thermal throttling or a specific code bug.

See docs/findings/windows-periodic-stalls.md for details and earlier tests.

## What the diagnostic build measures

On normal exit, the game logs its worst 64 intervals over 20 ms:

- cpu: scheduled game-thread kernel+user CPU time via GetThreadTimes. Windows
  accounting is coarse (often 15.6 ms steps); some values exceed individual wall
  intervals. Assess groups of frames rather than treating each value as exact.
- ow / ww: host wall time inside offscreen/window Vulkan fence waits. These are
  not GPU timestamps; they include OS/driver scheduling and overlap render/present
  fields. Do not add all columns together. Other waits such as image acquisition
  are not independently timed.
- cyc: QueryThreadCycleTime delta. This is OS cycle accounting, not a hardware
  clock sensor; cycles divided by CPU time does not prove exact CPU frequency.
- `--profile FILE` writes the full stack profile and `FILE.slow.txt`. The latter
  selects samples after the current vblank interval has exceeded 33.4 ms. It
  excludes the initial 33.4 ms, includes loading/menu waits, and can be empty.
  Both profiles share the same sampler and stack walks.

Raw stack addresses need the exact build's linker map. The Linux workstation
retains the diagnostic-2 map privately at
`work/diagnostics/windows-level3-native/frame-diagnostics-2/army-men-sarges-heroes-recomp.map`.
Ask the owner to transfer it if further symbolization is needed. Do not use a map
from another version/region. Existing profiles were USA/NTSC, not PAL.

## Next native test: telemetry alongside gameplay

Keep the laptop plugged in; keep baseline settings unchanged: 4x, FPS 60,
Standard latency. Record actual settings, Windows version and CPU model first.
Do not silently switch power plans. Record the current plan:

```powershell
powercfg /getactivescheme > power-plan.txt
```

Use one PowerShell window in the diagnostic game folder for telemetry:

```powershell
$counters = @(
    '\Processor Information(*)\% Processor Performance'
    '\Processor Information(*)\Processor Frequency'
    '\Processor Information(*)\% Processor Time'
    '\System\Processor Queue Length'
)
Get-Counter -Counter $counters -SampleInterval 1 -Continuous |
    ForEach-Object {
        $_.CounterSamples | Select-Object Timestamp, Path, CookedValue
    } |
    Export-Csv -Path cpu-telemetry.csv -NoTypeInformation
```

Counter names are localized and availability can vary. If this errors, inspect
`Get-Counter -ListSet *` and select valid local paths; do not ignore the failure.
An elevated shell may be needed for some counter sets. These counters can expose
performance changes but do not directly measure temperature or establish a
thermal/power limit. One-second sampling can miss short spikes.

From another PowerShell window in that folder:

```powershell
Get-Date -Format o | Set-Content gameplay-start-time.txt
.\army-men-sarges-heroes-recomp.exe --launcher --profile cpu-profile-telemetry.txt --profile-interval 10000 > gameplay-log-telemetry.txt 2>&1
```

Have the owner play the levels 1–3 route until slowdown occurs and note elapsed
minutes or wall-clock time. Quit the game normally, then stop telemetry with
Ctrl+C. Use unique run folders/filenames rather than overwriting prior evidence.
Keep all six outputs together:

1. cpu-telemetry.csv
2. power-plan.txt
3. gameplay-start-time.txt
4. gameplay-log-telemetry.txt
5. cpu-profile-telemetry.txt
6. cpu-profile-telemetry.txt.slow.txt

The start marker is before the launcher opens; use it as approximate alignment,
not an exact mapping of guest frame number to wall time. Guest time can fall
behind during stalls. If correlation is ambiguous, add explicit wall-clock
markers for burst onset before attributing a CPU-frequency change.

Compare per-core performance/frequency, CPU utilization and queue length near
reported bursts. Look for evidence before choosing a code change or power/thermal
hypothesis. If telemetry does not distinguish them, consider a Windows trace with
CPU scheduling/power events, rather than another aggregate stack-only rerun.

## Other reports and fixed issues: keep separate

The current reporter describes Windows 10 Home 22H2 (19045), Ryzen 9 3900X,
RTX 3090, driver 32.0.16.1692, 63 GB RAM. Their runtime resolution is unknown.
The supplied report-154731-ARMY0908.log is a redumper disc-reading log, not a
runtime profile. Do not infer their runtime cause from our laptop measurements.
A separate older RTX 5080/9800X3D report had different timing behavior.

Windows local tail dispatch was already enabled by default in f172776 and
published in v0.1.8-pal-preview.2. It fixed earlier exception overhead; these
remaining stalls occur with that fix active. Do not rebrand diagnostics as a fix.
Linux defaults remain different. Wine smoke tests verify startup/instrumentation,
not native Windows performance parity. USA opening cinematics legitimately lack
PAL's subtitles. Occasional PAL subtitle flicker is a separate unresolved issue.
