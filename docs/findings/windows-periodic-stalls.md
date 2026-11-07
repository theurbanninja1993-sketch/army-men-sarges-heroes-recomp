# Windows laptop periodic stalls (2026-11-07)

Status: investigating; no runtime fix claimed. Both USA and PAL are affected on the
user's laptop. The executable and disc image are on its internal SSD and it is
plugged into power. Renderer: NVIDIA GeForce RTX 3050 Laptop GPU, Vulkan swapchain.

## Native observations

All tests used exact 59.94 timing after the initial report. Logs supplied by the
user on removable media (the game itself runs from internal storage):

| Run | Scale | FPS setting | p99 interval | Worst interval | User report |
| --- | --- | --- | --- | --- | --- |
| gameplay-log.txt | 1x | 60 | 38.23 ms | 85.0 ms | smooth |
| gameplay-log-test2.txt | 4x | 60 | 144.85 ms | 398.5 ms | stalls, especially early cutscenes |
| gameplay-log-test3.txt | 2x | 60 | 37.14 ms | 84.8 ms | smooth |
| gameplay-log-test4.txt | 2x | Auto | 141.23 ms | 931.6 ms | stalls |

Every log reports zero audio drops/underruns and zero untranslated calls. These
aggregate counters do not identify the stall cause or prove audio timing correct.
Do not infer a resolution-only or interpolation regression from these runs:
Auto explicitly reports a 60 Hz display and interpolation **off**. In Live::start,
Auto resolves to false before creating interpolation targets/presenters or running
frames. The only remaining Auto flag is consulted in that startup check. An
intermittent stall can therefore explain the apparent setting correlation.

## Diagnostic build

The old Pacing log retained the first 64 intervals over 20 ms, which startup FMVs
filled before the severe stalls occurred. It now retains the worst 64 intervals
throughout the run and prints them in frame order. Each record also includes CPU
wall time inside the normal Live::present call (`pr`) and input polling/control
reading (`i`), alongside existing renderer/texture decode times. Presentation
includes its internal rendering and waiting; these columns must not be added as
independent CPU/GPU durations. They are not GPU timestamp measurements. Menu pauses
and scene loading can still appear as long intervals and need user context.

No changes to FPS selection, interpolation, simulation, or presentation behavior.
Synthetic host check: fill 64 slots with 25 ms frames, then inject a 200 ms frame;
the later frame survives with its associated counters and present/input times,
and a later 21 ms interval cannot evict it. Full Windows build and Wine startup
validation are recorded in the workspace HANDOFF.md.

Next native check: use diagnostic ZIP, 2x / Auto / exact 59.94, repeat same opening
and gameplay until the stall occurs; quit normally to emit the report and provide
its log. A smooth run is not proof of a fix. Keep the original package for A/B.
