# Duskbloods network test: boot, gameplay and image-quality investigation

Updated: 2026-11-11. This is a local engineering record, not a claim of complete game
compatibility. No commits, pushes or pull requests were made during this investigation.

## Current result

The rebuilt `build/bin/eglantine` reaches the private-server online menu, loads Continue,
renders the opening room/cutscene and reaches controllable third-person gameplay with the HUD.
The first successful candidate ran for 300 seconds and shut down normally. Its final live
status recorded 15,181 presents and approximately 60 FPS. A separate ten-second sample during
the first 3D scene measured 53.2 FPS; that sample included a 556 ms stall, so it does not
establish consistently smooth frame pacing.

The scene-blur follow-up found a depth-format mismatch: Duskbloods supplies `S8Z24`, but
both the prepass reader and texture-cache path expected `Z32`/`D32_FLOAT`. Accepting the actual
depth format allows FSR 3.1 to run. The `depth24-fsr` run logged **1108x624 -> 1920x1080** and
reached controllable gameplay. The owner confirmed that the blur was gone in this run, while
some jitter remained. One ten-second gameplay sample held **60.0 FPS**, with median 16.9 ms,
p95 20.0 ms, p99 20.1 ms and worst 21.1 ms. This is one sample, not a claim of locked 60 FPS
throughout the game. The run ended normally after 229.0 seconds including cleanup.

The primary display is 100 Hz with variable refresh disabled. Immediate presentation was the
owner's setting. A live FIFO comparison measured 52.8 FPS (worst 158.2 ms); it did not show an
improvement and Immediate was restored. The player moved between samples, and mode recreation
can stall, so these numbers do not isolate presentation cost. A further bounded `vrr-fifo`
run temporarily enabled variable refresh and FIFO. The owner then clarified that walls
flicker/move even with the character still, like a warped scene. This is reconstruction
instability rather than merely uneven presentation. VRR was restored to Never, Immediate
presentation restored, and the run stopped cleanly at 186.1 seconds. **Residual scene warping
remains unresolved.** No global refresh-rate change was made.

Earlier `temporal-reason` evidence explicitly confirmed the custom temporal stand-in, with
49.5 FPS over ten seconds. Its `plain copy` log only described missing sRGB decoding, not the
reconstruction method; `blitted` was similarly ambiguous. The inference that it proved a
bilinear fallback was unsupported. The completion log now distinguishes temporal and bilinear
paths. That run also logged Fermi 2D source-depth assertions at 251.37 s and device loss at
253.40 s, requiring forced cleanup. The first-scene crash is bypassed; longer stability is
not established, and the later fault has not been proven fixed by accepting depth.

All runs described here use local private test servers. Successful authentication with those
servers does not establish connectivity to Nintendo's or FromSoftware's public services or
multiplayer compatibility.

## Checkout and machine

- Eglantine branch: `duskbloods`, base HEAD
  `87ed20643a7677dbde5ad86df6abe8257f619543`.
- Title: `0400D1D02399A000`, The Duskbloods: Network Test.
- Host: Linux, Ryzen 7 5800X, Radeon RX 9070 XT, RADV GFX1201.
- The successful run used the owner's graphics settings: Vulkan, Fast GPU accuracy, docked,
  1x resolution, asynchronous shader compilation, and force-max-clock enabled. The new title
  fence policy and DLSS replacement were automatic; no frame-ahead or tensor override was set.
- Tests use separate configuration, NAND and save directories. The native
  `~/.config/eglantine/qt-config.ini` was not changed; its recorded SHA-256 is
  `3e14c132363f060f6ba55ca75f3aad9af0756b4eca9e7651932a081353f2f1e9`.
- The working Lichii comparison is the local `duskbloods` branch at
  `0143f6dd435d32a45c541341447924cb5e051f9e`. The Lichii checkout itself was on `nx2`
  during this investigation; the relevant branch was inspected with `git show`, without
  switching or editing that checkout.

## Earlier boot work already present

The detailed November 10 record remains in
[`build/bin/duskbloods-comparison.md`](../../build/bin/duskbloods-comparison.md), with the
corresponding Lichii findings in that checkout's `build/bin/duskbloods-findings.md`.

That work established two startup blockers before rendering:

1. The original application-memory budget selected a larger game heap profile without enough
   backing for it. The game failed its `M`/236 MiB allocation, then its `H`/500 MiB allocation
   as experimental budgets increased. A sufficiently backed larger title-specific pool cleared
   those panics.
2. Zero answers to Switch 2 remote-memory queries made the SDK report zero graphics memory.
   The render-target factory returned error 5 and the game subsequently called a null render
   object. Backed remote-memory reporting cleared this blocker and reached the title screen.

The eventual branch replaced the preliminary local memory experiment with the existing Lichii
boot fixes. Before this MCP session, it already contained the title-specific pool growth,
remote-memory/GPU-only-region support, private-server redirect, reactive flushing, TLD4/IMAD
lowering changes, and volume-slice clear changes. The earlier port explicitly omitted Lichii's
Duskbloods DLSS replacement because the two emulators organise their upscalers differently.
That omission became central to the remaining first-scene fault.

## MCP and test harness

The tree already contained an uncommitted integration of the built-in MCP server from commit
`13d34459cd29b8e61284452c65e783ea0bc33fc5`, including some rejected-patch artifacts. Those
pre-existing changes and unrelated untracked tests/helpers were preserved.

The GUI, SDL frontend and tests were built with the MCP integration enabled. Tests connect to
Streamable HTTP MCP at `http://127.0.0.1:6544/mcp`, with writes enabled only for these authorised
test instances. The older NX2 debug bridge on port 6545 was also used for baseline inspection.

Useful tools were `status`, `env_switches`, `press_buttons`, `set_stick`, `release_all`,
`perf_sample`, `unimplemented_commands` and `screenshot_raw`. The normal `screenshot` tool
returned black images despite visible rendering. `screenshot_raw` returned usable 1280x720
game-only captures and was used thereafter; the regular screenshot issue was not fixed here.

The initial private-server harness failed before emulator launch because system Python lacked
`grpc` and `cryptography`. It was run with the existing launcher virtual environment instead.
The local launcher starts an NPLN stub and TLS front door, uses a separate seeded NAND/save,
and redirects the game to loopback. Server payloads/certificates are not included in this doc.

Local helpers are under `/tmp/duskbloods-mcp-work/`:

- `run.py`: visible GUI launch, copied settings, private servers, wall-clock bound, cleanup,
  run metadata and emulator-log preservation. `--env` and `--setting` support A/B experiments.
- `call.py`: MCP requests, full structured JSON output and image saving.
- `navigate.py`: a short title/login/Continue input route with raw-screen checkpoints.
- The title/login sequence is asynchronous; a final manual MCP A press was sometimes needed
  after the online menu appeared. The script is not a general gameplay route.

Every owned emulator run was bounded, normally 100–300 seconds, and only these test instances
were stopped. Failed GPU runs sometimes required the harness's final kill after its graceful
shutdown timeout. The successful gameplay run exited normally.

## Experiments and findings

| Experiment | Observation | Consequence |
| --- | --- | --- |
| Fresh original binary, default settings | About 60 FPS at title, then invalid methods/MME errors and frozen presentation around 31 s | Established a fresh baseline, not just an old log |
| Fetch drain plus verification | Passed the earlier title freeze; title input reached a BCAT error | Useful diagnostic, not shipped as a global forced drain |
| Macros on GPU back thread (`EDEN_NX2_SPLIT=1`) | Still produced macro errors | Not selected as a fix |
| Experimental GPU optimisations disabled (`EDEN_NX2_PERF_SAFE=1`) | Still corrupted commands in a title run | Did not establish a safe-mode solution |
| Old split-method parking heuristic | Ran longer but later produced parameter FIFO errors | Not enabled by default |
| Wait for the current present fence (`EDEN_NX2_FRAMES_AHEAD=0`) | Title/login remained stable; settled sample 59.9 FPS | Added Duskbloods to the existing title-specific current-frame policy |
| BCAT file-name correction | Regulation file opened and was read; private authentication completed | Cleared error 2122-0001 observed during calibration retrieval |
| First Continue with original shader support | Missing bindless image operations, uniform local spill and U64 shift; then device loss | Captured complete programs and ported narrow lowerings |
| Shader-lowering candidate | Cleared the instruction-level gaps, but first 3D scene still lost the device | Lowering success alone was insufficient |
| High GPU accuracy | Same first-scene device loss | Not fixed by accuracy setting |
| Synchronous shaders plus conservative optimisation settings | Same first-scene device loss | Not fixed by waiting for shader compilation |
| Clock boosting disabled | Same first-scene device loss | Clock-boost shader was not the sole cause |
| RADV hang capture, two devices | Report included the emulator's boost compute shader | Initially misleading; separate renderer capture was needed |
| RADV hang capture with boost device disabled | Main faulting compute used color at cbuf0+0x328 and outputs +0x348/+0x350, with large software tensor lowering | Identified the game's DLSS final output pass |
| Adapted Duskbloods replacement | Passed the former crash, rendered opening scene and controllable gameplay | Current working candidate, with blur/jitter follow-up pending |
| Explicit temporal-path verification | Custom temporal upscale logged at 62.20 s; 49.5 FPS in a separate 10 s sample; later device loss at 253.40 s | Temporal execution confirmed; a separate later stability issue remains |
| Depth contract capture | Native depth format 43 (`S8Z24`) was rejected by the 32-bit-only checks | Identified why FSR never ran |
| Accept actual depth format | FSR 3.1 logged 1108x624 -> 1920x1080, controllable gameplay; owner reports blur gone | Confirmed image-quality improvement, residual jitter remains |
| Live FIFO, VRR disabled | 52.8 FPS sample after a 60 FPS Immediate sample, different player movement | No isolated benefit established; Immediate restored |

The main-renderer capture is retained locally at
`/home/mcm/radv_dumps_46147_2026.11.11_09.43.48`. The earlier two-device capture is
`/home/mcm/radv_dumps_44718_2026.11.11_09.38.17`. These contain driver/shader data and remain
outside the source tree. The main capture, not the boost-only pipeline report, is the evidence
for identifying the DLSS failure.

## Source changes made in this investigation

### BCAT regulation file

`src/core/hle/service/bcat/delivery_cache_file_service.cpp` now validates a file name with
`VerifyNameValidFile`, retaining directory validation for the directory. The former directory
rule refused the period in `regulation_nt_1_0.bin`. This is the existing Lichii BCAT fix
(`0dd7c1d554`), not a game-file patch. A live debug-filtered run verified `Open`, `GetSize` and
`Read` of the regulation file, followed by successful private-server requests.

### Present fence policy

`src/core/hle/service/nx2_gsv.h` adds this title to the existing list that waits for the current
frame fence before returning present. This prevents the CPU from reusing data while the GPU
still consumes it. Other titles retain their existing selection and the
`EDEN_NX2_FRAMES_AHEAD` diagnostic override remains available.

### Shader lowering

`src/shader_recompiler/frontend/ampere/lower_to_maxwell.h` gains:

- Register-handle formatted image load/store encodings `0x998`/`0x99c`, through the existing
  Maxwell bindless image path, with guarded dimensions, masks, registers and reserved bits.
- Uniform-addressed local spills, uniform-bank detection and corresponding register-pressure
  operand coverage.
- The captured unsigned U64 left-funnel shift through 32 bits.

These were adapted from local Lichii implementations, including register-handle surface commit
`cf1beed4beaa75317b1ca6770c196c4616d6e518`. Tests in
`src/tests/video_core/ampere_lowering.cpp` retain representative captured instruction words and
check emitted operand fields and invalid forms. No complete game shader is added to source.

### Duskbloods DLSS replacement

`src/video_core/engines/kepler_compute.cpp` adapts the missing working-fork behaviour to
Eglantine's existing DLSS chain:

- Recognise the prepass by its captured motion/network stores at `0x290`, `0x2a4`, `0x2b0`.
  Constants alone are insufficient because intermediate passes can retain them.
- Preserve the prepass, and skip previously identified internal network programs while the
  replacement is selected.
- Replace the final output before dispatching its faulting software tensor implementation.
- Accept full-size input/output targets with a smaller rendered region, read the inclusive
  last-column/last-row fields at `0x39c`/`0x394`, and reject invalid or out-of-bounds regions.
- Use Duskbloods depth at `0x288` and reverse its motion vectors (`-1,-1`).

`src/video_core/duskbloods_upscale.h` contains the checked region calculation. Tests alongside
the other upscale tests cover docked/handheld-sized targets, smaller motion backing, each
target bound, zero extents and `UINT_MAX` inclusive coordinates.

The optional region is passed through `rasterizer_interface.h`, Vulkan rasterizer and texture
cache. The temporal fallback's push constants and `temporal_upscale.frag` now accept the render
size and use the same pixel grid for differently sized targets containing that region. Existing
callers retain the full-input default. The `temporal-reason` run confirms that the custom
temporal stand-in executes; FSR 3.1 was not logged. Image quality is still unverified. Bounded
temporary diagnostics did not report an image/view refusal in that run. The template is
explicitly instantiated in `renderer_vulkan/vk_texture_cache_base.cpp`; recompiling only
`vk_rasterizer.cpp` does not rebuild the implementation. This was corrected before the run.

The two identified native programs are documented in
`tools/nx2/catalog/0400D1D02399A000.json`: prepass `e5efe04581720f15` and final output
`50e47e31c31fc1f8`. Catalogue entries contain names and short evidence notes only.

### Depth-format follow-up for blur

A bounded input-contract log in `temporal-contract` showed valid Halton jitter and
1920x1080 colour/motion images, but rejected depth with native format 43 (`S8Z24`), also
1920x1080. The existing Fortune's Weave path required `Z32`, and the texture cache then
required `D32_FLOAT`. Consequently the game could never reach FSR 3.1 with this depth format.
The implemented follow-up accepts `S8Z24` for Duskbloods, uses the existing native descriptor-view
normalisation, and creates a depth/stencil view in the actual image format. FSR samples the
depth aspect through the existing `DepthView()` method. Other titles retain their prepass
format selection. `depth24-fsr` confirmed successful FSR execution and the owner confirmed
that the previous blur was gone. Residual jitter remains; this does not prove its cause.
The completion log now distinguishes `temporal`, `bilinear copy` and `not accelerated`.

`temporal-contract` reached gameplay and shut down normally after 145.9 seconds; a twelve-frame
still-camera sequence is retained for comparison.

### Clock-boost workload pacing

`src/video_core/renderer_vulkan/vk_turbo_mode.cpp` uses the pre-existing local
`TurboModePacer` helper, whose tests are now included in `src/tests/CMakeLists.txt`.
The old wait predicate could immediately succeed during normal activity and repeatedly submit
the clock-boost workload. Desktop boosts now run only in short submission gaps, at bounded
intervals, and stop after prolonged inactivity. A transfer-to-compute memory barrier follows
the buffer fill, and fence failure ends the boost loop. Android's existing path is retained.

This is a separate scheduling/correctness improvement. Disabling boost alone did not clear
the game's DLSS fault, and no isolated FPS gain from pacing has been claimed.

## Validation

- GUI `yuzu` target (`build/bin/eglantine`), `eden-cli` and `tests` rebuilt successfully.
- The existing Ninja dependency log repeatedly reported truncation and scheduled large
  unnecessary rebuilds. Final isolated changes were compiled with their exact commands from
  `build/compile_commands.json`, then the normal archive/link commands were used for all three
  binaries. Source/object timestamps were checked so the final prepass guards were included.
- Captured program corpus: **207/231** lowered before the instruction fixes; **231/231** after,
  using the offline tool's tensor-emulation option. Every one of the 207 previously successful
  programs retained an identical lowered-code hash. This does not prove every graphics stage
  or pipeline is supported; native tessellation stage 2 is still rejected in the runtime log.
- Focused combined suites: **23 cases / 2,790 assertions passed** for Duskbloods, clock pacing,
  MCP and existing Ocarina upscale tests.
- Final full suite: **396 cases, 391 passed, 5 failed; 14 failed assertions**. The failing
  assertions are the same existing `ampere_logged_numeric.cpp` failures recorded before the
  shader changes (lines 183, 188, 192, 236, 266, 272, 402, 470 and 472). The first sandboxed
  full-suite attempt additionally lacked socket permissions; the reported final result is the
  unrestricted run, not that sandbox failure.
- `git diff --check` passes.
- Successful runtime reached the opening scene and gameplay, accepted injected movement/camera
  input, exceeded 15,000 presents, and ended without a device-loss report in that run.
- After accepting depth, all three binaries were linked again and the focused suites passed
  again. The full suite again produced the same 5 failures / 14 assertions (396 cases total).
- Long-term stability and handheld mode remain unverified. The owner confirmed the blur fix,
  while remaining jitter is being checked separately.

## Development constraints

The referenced `docs/policies/AI.md` and `Coding.md` were absent from this uninitialised docs
submodule. The root AGENTS instructions were followed: no push/PR, no changes to the Lichii
checkout, bounded owned runs, live MCP inspection and input release. Existing dirty MCP work
and rejected-patch artifacts were retained. No commits were made.

## Documentation location

The source-review copy is `tools/nx2/duskbloods-progress.md`. The first draft was written under
`docs/developer/`, but `docs` is an uninitialised Git submodule here, so files there do not appear
as ordinary changes in the parent repository. Copies are also kept in `docs/developer/` and
`build/bin/` for local convenience.

## Evidence and replay

Curated logs, test results, metadata and captures are stored under
[`build/bin/duskbloods-evidence/mcp-2026-11-11/`](../../build/bin/duskbloods-evidence/mcp-2026-11-11/).
The live working directory `/tmp/duskbloods-mcp-work/` retains additional experiments,
native dumps, the private test data and the currently running diagnostic. Do not commit those
game dumps or private-server data.

For this PC, launch a bounded diagnostic using the existing private-server setup:

```sh
/home/mcm/.local/share/duskbloods-launcher/venv/bin/python -I \
  /tmp/duskbloods-mcp-work/run.py --name another-check --seconds 300 --mcp
```

Use a new run name. That helper depends on the existing local playDB-CNT launcher and seeded
test data; it is not a portable distribution script. With the test running, the repository's
MCP client can inspect and drive it:

```sh
python3 -I tools/mcp/scripts/mcp.py status
python3 -I tools/mcp/scripts/mcp.py screenshot_raw --save-image /tmp/dusk-screen.png
python3 -I tools/mcp/scripts/mcp.py press_buttons '{"buttons":["A"],"hold_ms":300}'
python3 -I tools/mcp/scripts/mcp.py perf_sample '{"seconds":10,"screenshot":false}'
python3 -I tools/mcp/scripts/mcp.py release_all
```

To end a helper-owned test early, create `<run-directory>/stop`; the harness stops its emulator
and local servers. Never apply that cleanup to an emulator the user launched separately.

## Next check

The owner requested a handoff. Read `HANDOFF_DUSKBLOODS.md` in the repository root. All owned
runs are stopped and the display policy restored. Optional live jitter-sign/zero-jitter/
spatial-only/zero-motion probes were compiled but never run; they are saved as
`live-jitter-probes.patch` in the evidence directory and excluded from the working source.
Next, test temporal inputs against a still wall. Reproduce the later device loss separately;
do not conflate the confirmed blur improvement with a stability fix.

### Motion-vector scale (the "walls moving" defect), 2026-11-11

Fixed, and confirmed by the owner on the fixed build. The upscale read the prepass's motion store
(cbuf0+0x290) as UV offsets with sign -1. For this game the prepass has already multiplied the
raw velocity target (cbuf0+0x284) by the game's motion scale (cbuf0+0x198/0x19c, -1080 on both
axes), so the store holds offsets to the previous position in output pixels. FSR 3.1 received
vectors about a thousand times too long. `UpscaleInsteadOfNativeProgram` now passes
`{1 / output width, 1 / output height}` as the motion scale for this title.

How it was found, in order:

1. Live knob matrix on a still camera (jitter negated, jitter off, x only, no motion, spatial
   copy, stand-in). Only "no motion" helped: instability 1.44 to 0.70, sharper too. Jitter and
   depth were therefore not the cause.
2. Reading the motion image back: zero over static scenery on most frames, garbage of tens to
   thousands of pixels on the character and loose debris, and occasional whole-screen vertical
   kicks. The colour input did not move on those frames.
3. The prepass constants did not differ on the bad frames; the game's raw velocity target was clean
   (nothing above 0.6 render pixels standing still). A least-squares fit gave output = raw x -1079.5
   on both axes, and the constants hold -1080, -1080.
4. Scale and sign candidates on live knobs, still and during a pan: output pixels with a positive
   sign was as steady as no motion standing still and the only one that stayed sharp in the pan.

Depth convention checked on the way: clear depth 0, test GEQUAL (reversed), as FSR is built for.
The table, scripts and the probe sources are in
`build/bin/duskbloods-evidence/mcp-2026-11-11/motion-scale/`. Focused tests
(`[duskbloods],[turbo_pacer],[mcp],[ocarina_upscale]`) pass: 23 cases, 2,790 assertions. The full
suite was not rerun for this one-expression change.

Open: about 40 FPS in the graveyard scene (not root-caused; neither GPU thread was saturated in
the sample), and handheld mode, resolution scaling and fast motion are unverified with the new
scale. See `HANDOFF_DUSKBLOODS.md`.

### After the handoff, 2026-11-11 afternoon (reconstructed, then continued)

Three pieces of work were done after the 11:44 handoff without being written down. They were
reconstructed from the diffs and the run directories under `/tmp/duskbloods-mcp-work/`:

- **FSR images kept across render-size changes** (`vk_fsr3_upscaler.{h,cpp}`,
  `vk_texture_cache.cpp`; runs `fsr-recreate*`, `fsr-max-size*`). The game renders a region of a
  1920x1080 target and changes the region's size (dynamic resolution). The upscaler recreated every
  image, and lost its history, whenever the size changed. It now allocates for the target's full
  size (`Inputs::max_render_size`) and only recreates when that is exceeded or the output size
  changes. The log line reads `FSR 3.1 upscaler: 1920x1080 -> 1920x1080` as a result; the
  per-frame render size is still the region (`DLSS replaced by FSR 3.1: 1108x624 -> ...`). The
  owner's verdict on this change was not recorded. Screenshots named `flash`, `green` and
  `fire-missing` in those run directories suggest reports of a flash on recreation, a green tint
  and a missing fire effect; none of those is confirmed fixed or still open.
- **3D-texture 2D blits** (`fermi_2d.cpp`; runs `deep-blit*`). A temporary log line (marked PROBE,
  still in the source, to be removed) describes the blits that trip `Source depth is not one`.
  They are 1:1 copies of a 16x16x16 RGBA16F volume (format 0xca, block depth 4) done one slice per
  blit: the game passes base + z * 0x200 as the address with layer 0, which is where slice z starts
  in a block-linear volume of this size. Both the software blitter and the texture cache copy
  slice 0 at the given address, which is the slice intended, so the assert is noise rather than a
  wrong copy. `SwizzleSubrect` stops after one slice, so the one-slice buffers are not overrun.
  The software path does read and write back the full 16-slice span from the offset address
  (up to 0x1e00 bytes past the volume, rewritten with what was just read).
- **Device loss**: two runs (`temporal-reason` at 253.4 s, `deep-blit` at 68.5 s) lost the device
  2.03 s and 2.06 s after a burst of these blits. Four other runs logged 448 to 4,016 of the same
  asserts with no loss. The link is not established and no driver capture exists for either loss.

Frame rate, measured in run `perf-threads` (intro cutscene, 60 FPS target):

- No emulator thread is saturated: the six guest cores sit at 32 to 45% of a host core each, the
  GPU front and back threads at 34 and 39%.
- The guest is blocked on the present fence for 11.1 to 11.4 ms of every 16.7 ms frame even when
  60 FPS holds, and 12.8 ms of a 41.7 ms frame in the slowest logged scene.
- The host GPU (`gpu_busy_percent`) reads 71 to 76% in that cutscene. `force_max_clock` is on in
  the owner's config and its boost shader counts as load, so this number does not yet show whether
  the host GPU is the limit. The setting only changes at boot, so it needs a separate run.

### Colours going dark and green between areas (fixed), 2026-11-11

The owner reported the colours "inverting" for about a second in one spot. Recorded while they
walked through it: the picture goes dark with a green tint for roughly a second on crossing a
doorway, and the log shows a burst of the 3D-texture blits described above at the same moment.

Cause. The volume is the game's 16x16x16 RGBA16F colour grading table. While the grading blends
from one area's to the next, the game copies the blended table into the one the tone map samples,
every frame, one slice per 2D blit, giving each slice's own address (base + z * 0x200) and the
volume's depth. The texture cache treats such a surface as a one-slice 3D image at that address.
When the destination volume is not cached it creates 16 separate 16x16x1 images inside the
volume; when it is, the blits name the right slices but the sampled table is still wrong for the
length of the blend. Why the second case fails was not traced.

Fix (`fermi_2d.cpp`, `sw_blitter/blitter.cpp`): a blit whose source or destination has depth
above one goes through the software blitter, which copies the slice in guest memory where the
address says, and the texture cache picks the change up from there. The blitter's span for such a
surface now ends with that slice instead of covering the whole volume from the slice's address,
which ran up to 0x1e00 bytes past this volume. The `Source depth is not one` assert is removed.

Verified live with a debug knob while the owner walked back and forth (run `lut-probe`):
texture cache path 5 of 6 transitions with green frames, software path 0 of 15. Evidence and the
table: `build/bin/duskbloods-evidence/mcp-2026-11-11/colour-invert/README.md`. The final build
(the same copy without the knob, probes removed) was built and passes the focused tests (23
cases, 2,790 assertions) but was not played. No unit test covers the span arithmetic.

Cost: during a blend the 16 slices are read from and written to guest memory each frame, which
downloads the source volume when the GPU wrote it. No frame-time measurement was taken.

Device loss: both recorded losses came about 2 s after a first-frame burst of these blits, the
case that created the 16 overlapping one-slice images. That path is no longer taken. Whether the
losses stop is unknown; it needs runs, and a driver capture if one recurs.

### Test match: green patches, black blocks (diagnosed, part fixed), 2026-11-11

The owner reported green lighting and black squares in a test match. Driven and captured over the
MCP and debug servers in the owner's running session (run `lut-probe`, diagnostic build).

Seen: fluorescent green patches on some ground and on lit windows, black rectangular blocks near
foliage and walls (largest around a tree outside Euthania Cathedral), thin horizontal stripes over
the lower half of the player character. The City Streets area looked clean in four views.

1. **A fragment program was rejected in the match** (first use at 284.6 s, program
   0x2314472e00): `native texture gather mode [op f63 02087f63 30000409 081e15ff 000f4200]`.
   It is a gather with the texture handle in a uniform register and a packed offset (AOFFI), a
   combination the lowering refused, so every draw with that program was skipped. **Fixed** in
   `lower_to_maxwell.h`: the offset goes in R247 after the handle in R246 and Maxwell TLD4.B gets
   its offset mode. Test with the logged words in `ampere_lowering.cpp`; `[ampere]` still has only
   the 5 failing cases (14 assertions) in `ampere_logged_numeric.cpp` that failed before.
   Built, not yet run in a match, so which of the visible defects it accounts for is unknown.
2. **Green patches come from compute program 68a84477c245d545** (hash 4ad8bad52372f667).
   A frame capture showed its outputs, a half-resolution RGBA16F mip chain blurred three times a
   level, holding NaN (1,680 at 480x270), infinity and values up to 24,368, in G and A only; two
   passes later the lighting target (pass 22, second colour target) has 1,389 NaN pixels and
   green and magenta pixels where the screen shows the green patch. Rejecting the program live
   removed the patch. The program is 74 instructions: five taps, each weighted from constant
   buffer 1 (words 0x1a, 0x1e, 0x22, 0x26, 0x2a, i.e. bytes 0x68 to 0xa8), divided by the sum of
   the five weights. Its input is close to zero, so output of that size means the weight sum is
   near zero: the weights the program reads are wrong or missing. Not confirmed: after the live
   reject and restore the program never dispatched again in that session, so its constant buffer
   could not be captured. The same program is in the dumps from the first area.
3. Black blocks: not traced. NaN reaching the tone map is the usual cause and items 1 and 2 are
   both candidates.
4. Also seen in a capture, not followed up: a 960x540 R16G16F texture sampled by compute
   fb65c8f3da7678a8 holds 41,105 NaN and values of +/-65,504.

Next: a match on the build with the gather fix; if the green remains, capture with
`{"method":"capture_frame","params":{"buffers":true}}` while the patch is on screen (before any
reject) and read constant buffer 1 of 68a84477c245d545 at 0x68.
Both programs are in the catalogue.

### Test match: fire has no flames (under investigation), 2026-11-11

Owner's report at a lit brazier at Nether Gate Lake: the firelight is there, the flames are not.
Captured in the same session (run `lut-probe`, the build without the gather fix):

- The game draws particles into a 480x270 RGBA16F buffer (cleared to 0, 0, 0, 1), upsamples it to
  960x540 with more particle draws, then composites. Both buffers were exactly 0, 0, 0, 1 after
  seven and eight draws: nothing those draws rasterised was kept.
- The seven draws use vertex program ae1504bb159dc949 with fragment programs 833414760885c606,
  03f7dabc1cee030d and 4bdad791cb26baf8; two of them sample 2048x2048 BC4 textures. None of
  these is rejected and the log has no translation warning for them.
- Not established: whether the flames are those draws (and their output is lost) or belong to the
  fragment program rejected for its gather (item 1 of the previous section), whose draws are
  skipped and so never appear in a capture. The rejected gather is the fourteenth instruction of
  its program, which fits a soft-particle depth fetch. An earlier run named `fire-missing` has no
  gather rejection in its log, which argues against that, but what that screenshot recorded is
  not known.
- Uniform buffers and textures of that pass are kept in `/tmp/duskbloods-mcp-work/fire-capture/`
  (game data, not for the repository). Run `gather-fix` is the first on the build with the gather
  fix, with `EDEN_NX2_SHADER_DUMP=/tmp/duskbloods-mcp-work/match-dumps`.

### Tessellation stages lowered (port from Lichii), 2026-11-11

The missing flames are skipped draws, not empty ones. On the build with the gather fix (run
`gather-fix`) the gather rejection was gone and the brazier still had no flames; the particle
buffers held only four very large fog volumes (world scales of 425 to 2,801), and the log showed
draws with a rejected stage 2 program (tessellation control) skipped into one of those same
particle buffers (0x816d687000) and into the 1920x1080 scene targets. Eleven such programs were
rejected with `Unsupported native graphics shader stage 2`.

Ported from Lichii (`28cf8e2067`, `be5bff1d3b` on its `duskbloods` branch), adapted to this
tree's `LoweringOptions`:

- `lower_to_maxwell.h`: `TessellationStage` option; ALD/AST accept per-patch attributes and the
  control stage's output reads, the evaluation stage's tessellation coordinate read, and pass the
  patch and output bits to Maxwell; ISBERD vertex handles for tessellation stages as for geometry.
- `shader_environment.cpp`: tessellation control and evaluation programs are admitted when the
  header's stage matches the slot (a mesh program in the same slot is still told apart), and the
  native code is lowered after the stage is known.
- `src/tests/video_core/ampere_tennis_tessellation.cpp`: Lichii's tests, six cases, pass. The
  pipeline-side guards (`3a7477111f`) were already in this tree.

`[ampere]`: 85 cases, the same 5 failing (14 assertions) in `ampere_logged_numeric.cpp` as
before. The new test file is in `src/tests/CMakeLists.txt`; the build directory has not been
reconfigured (the object was compiled and linked by hand), so the next `ninja` run reruns CMake.

Run `tess` (first area, 90 s in): no program rejected, 3 draws skipped, all `build-deferred`, no
pipeline creation errors. Whether the flames and the black blocks are fixed in a match has not
been checked yet.
