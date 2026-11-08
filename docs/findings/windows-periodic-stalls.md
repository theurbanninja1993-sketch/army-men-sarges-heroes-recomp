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

## Stack profile and opt-in experiment (2026-11-08)

Native test6 collected 6,709 samples over70.94s at10,000us intervals, zero failures.
The exact test2 PAL map resolves1,499 samples (22.3%) through nonlocal_return;
1,485 (22.1%) include C++ exception throwing, and1,434 (21.4%) include tail_transfer.
Normal frame pacing sleep accounts for2,381 (35.5%) samples. These are inclusive,
aggregate wall-time sample shares, not additive CPU utilization or a timeline that
identifies which samples coincide with each stall. Native test6 still had a328.3ms
worst frame and93.51ms p99. This gives a concrete CPU overhead lead, not a proven
explanation for intermittent stalls.

Many nested overlay tail edges cannot use the existing top-level TailRunner because
indirect_depth differs. They throw NonLocalReturn to unwind to run_guest. The
experimental DREAM_LOCAL_TAILS=1 path installs a runner around each indirect call
and consumes only explicitly deferred tail_transfer targets there. NonLocalReturn
is never caught by these runners: real task switches/nonlocal returns still unwind
to run_guest. The option is disabled by default, so native A/B can use one package.

Focused control-flow tests run separately with the option off/on:100,000 nested
tail iterations, continuation inside a translated function, tail return to caller,
genuine nonlocal escape, and runner restoration after exception unwinding. Five
tests /16 assertions pass in each mode (five tests total). The normal path still dispatches through
find_function/resume_at, preserving overlay signature checks and hooks.

Use the experimental package with the environment variable set before launching:

```powershell
$env:DREAM_LOCAL_TAILS = "1"
.\army-men-sarges-heroes-recomp.exe --launcher --profile cpu-profile-test7.txt --profile-interval 10000 > gameplay-log-test7.txt 2>&1
```

Keep2x/Auto/exact59.94 and the same route. Exit normally and compare both logs.
Set DREAM_LOCAL_TAILS to0 or remove it to select the original dispatcher. No native
performance improvement or stall fix is claimed until the laptop confirms it.


The experiment keeps the existing dispatcher while in a delivered interrupt frame,
so interrupt tail/RTE bookkeeping uses its original unwind path. A resumed block
whose final PR differs from the original call return also forces nonlocal unwind.
These edge cases have focused tests. Windows-compiled versions of all five tests
pass with the switch off/on under Wine, as do the Linux-compiled tests.

Deterministic first-mission checks at8450 guest frames: USA full16MiB RAM identical
between off/on. PAL both reach phase3 with exactly matching camera pointer, player
position, yaw and pitch, zero untranslated calls, and zero task-switching RTEs after
narrowing the experiment to ordinary calls. PAL full RAM is not byte-identical: a
reference using the same VMU differs in195 bytes, with10 guest cycles difference at
the frame-bound stop. Full state equivalence and gameplay parity are NOT established.
An earlier broader prototype changed interrupt unwinding and was narrowed before
packaging. Wine PAL headless run87.47s off vs33.12s on; USA22.11s off vs22.83s on.
These timings are local observations, not native Windows benchmarks. The native
laptop must confirm stall behavior and game correctness. The option remains OFF by
default and is not proposed for a stable release yet.

## Native confirmation and Windows default (2026-11-08)

The user confirmed test8 was normal in gameplay/controls/cutscenes and test9 ran
well at4x. Test8 explicitly enabled local tails: p9918.48ms/max51.7ms,3of8844intervals
above33ms; exception-throw samples248/14139=1.75%. Inactive test7 was p9939.91ms/
max89.2ms,407of9332above33ms;throw samples2764/15807=17.49%. Initial inactive test6
had p9993.51ms/max328.3ms. Runs/routes differ, so these are observed improvements,
not a controlled benchmark isolating every source of intermittent stalls.

Active PAL4x test9: p9918.47ms,13of7546above33ms,zero untranslated/audio drops and
underruns;throw samples247/12173=2.03%. Native NTSC4x test10 was also reported normal:
p9918.17ms,5of8788above20ms and2above33ms,zero untranslated/audio drops/underruns.
Its1025ms maximum is exactly guestframe4438, where F11 capture wrote its files;
1019ms is inside presentation. Exclude that deliberate capture hitch from spontaneous
stall claims. Another169ms frame inside presentation remains in this run.

After these native checks, Windows defaults to local tail runners. Linux retains
the original default. DREAM_LOCAL_TAILS=0 selects the original dispatcher for A/B;
1 enables the optimized path on either platform. Empty/unset uses the platform
default. Unit checks cover0/1/unset on both Linux and Windows-under-Wine. Guest
control-flow semantics are unchanged from the opt-in test3 binary; only selection
and log wording changed. Broader Windows campaign coverage is still pending.

Intermittent PAL subtitle-box flicker remains open and also occurred while the
optimization was disabled. USA's missing opening subtitles are original disc
content, established separately in cutscene-subtitles.md; no subtitle fix was made.
