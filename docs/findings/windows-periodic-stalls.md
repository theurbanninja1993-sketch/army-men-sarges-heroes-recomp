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

## Later-level report investigation (2026-11-25)

The supplied report154731 attachment, `ARMY0908.log`, is a redumper optical-disc
read log. It reports a failed dump attempt, not game execution. It contains no
runtime build, GPU, FPS, frame pacing, cutscene, or audio statistics. It does not
establish that the image actually played is damaged. Hardware/settings from the
older report66081 must not be assigned to this different report.

### Distribution check

GitHub's live release list still identifies v0.1.7 as latest stable; the PAL
preview1 release is Linux-only. The public Windows v0.1.7 archive has SHA256
f1d8c505adfaa5490c5962612fa4561d9c8c7b78b402f059a01d2b5a29c31e00 and BUILD-INFO source
`dbbd29d2ab97af59d1b5bcfeac7d8ffbe7fa5a14`, matching the tag. Our local test4 archive
identifies source `f1727765ab702cf93dec39947f152990a180bf52`. The public archive was
verified against its GitHub asset digest before testing. It does not include the
new dispatcher/signature-comparison changes. This is an actionable distribution
gap, but does not prove which build the reporter ran or that this explains their
symptoms. No matching public GitHub slowdown issue was found among the repository's
current issues; the RecompLauncher report itself was not available here.

### Source audit for session growth

The Vulkan texture cache has an entry-count LRU threshold of1536 and trims toward
1024 entries; old overwritten allocations are retired and freed after the frame
ring delay. This bounds entry count, not GPU bytes or driver allocations. The SDL
audio sink has an8192-sample high-water limit and drops blocks rather than allowing
its source queue to grow indefinitely. These checks did not identify an obvious
unbounded queue/cache defect; they do not rule out leaks or hardware-specific
memory pressure.

### Reproducible transition checks

Compared the exact public Windows package and existing test4 package under Wine,
using the owner's USA CDI, isolated settings/cards, fixed RTC seed, native password
validation and native Exit Level actions. The route selects Bathroom3 (TDBWL),
exits to the native main menu, selects Riff4 (MSTRMN), exits, and selects Bathroom3
again. Each run advances28000 guest frames (467.101 guest seconds). This is a
transition/startup check, not completion of campaign missions or prolonged combat.

Both windowed packages completed the route, with two returns to the native main
menu and final RAM showing gameplay phase3/level3. Both reported zero untranslated
calls, zero texture decode failures, and808 cumulative texture decodes. Native
menu traces verify all three password/loading paths. Wine-process resident memory
at sampled loading/menu boundaries rose modestly on the new level and then held:

| Package | First Bathroom load | Riff load | Return to Bathroom |
| --- | ---: | ---: | ---: |
| Test4 candidate | 210340 KiB | 214456 KiB | 214456 KiB |
| Public v0.1.7 | 213496 KiB | 216736 KiB | 216740 KiB |

These are Linux `/proc` RSS observations for the Wine game process, not native
Windows working-set or GPU-memory measurements. The short route did not exhibit
large continuing growth. Candidate/public displayed741/27982 frames respectively
under the local desktop compositor, so their windowed elapsed times are NOT a
valid performance comparison. Their guest logic/render work still advanced, but
presentation differed. Xvfb was attempted and rejected because this host's virtual
X server lacks DRI3 Vulkan presentation; that is a test-environment limitation,
not a game failure. A headless repetition isolates guest CPU execution from this
presentation confound; its final results are in the workspace handoff/evidence.

The tests disable audio playback and frame interpolation and run unthrottled. They
cannot establish audible cutscene quality, native Windows frame pacing, high-refresh
interpolation behavior, VRAM pressure, or state carried by actually completing the
preceding missions. No new runtime fix is justified by these results alone. The
reporter trying the newer package and supplying its actual runtime log remains the
most useful discriminating check.

Headless repetition completed for both packages: same28000-frame route, final
phase3/level3 and zero untranslated calls. Runtime-reported host execution was
123.25s for the candidate and123.74s for public v0.1.7. These essentially equal
Wine timings do not establish a later-level CPU regression or predict native
Windows performance. Host system load was not controlled. No unsolicited menu
actions appeared in either final scripted route. Both the renderer-active and
headless checks passed; audible cutscene playback and actual mission completion
remain outside this check. Raw evidence and repeatable scripts are retained under
`work/diagnostics/report-154731-later-levels/` outside the source Git repository.
