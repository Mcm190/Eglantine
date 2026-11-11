# Duskbloods handoff — 2026-11-11

Start here, then read [the full engineering record](tools/nx2/duskbloods-progress.md).
The owner requested this handoff because the session is near its limit. Work is saved locally;
no commits, pushes or PRs were made.

## Read this first: state on the afternoon of 2026-11-11

Work continued after the text below was written. `tools/nx2/duskbloods-progress.md`, section
"After the handoff", has the detail. In short:

- Uncommitted and not in the list below: FSR images kept across dynamic-resolution changes
  (`vk_fsr3_upscaler.{h,cpp}`, `vk_texture_cache.cpp`), owner's verdict unknown; and the colour
  grading fix (`fermi_2d.cpp`, `sw_blitter/blitter.cpp`): blits of volume slices go through the
  software blitter. No probe code is left in the source.
- **Fixed: colours going dark and green for a second between areas.** A/B confirmed with the
  owner walking through it (5 of 6 transitions bad before, 0 of 15 after). The final build without
  the debug knob has not been played.
- `build/bin/eglantine` SHA256: `26ff6103dbb54bc785c73ea6a6668144099936cee4a98b501b9a8c9870f2ab68` (gather fix and tessellation port).
- Test match graphics (green patches, black blocks): a rejected fragment program (gather with
  a uniform handle and a packed offset) is fixed in `lower_to_maxwell.h`, built and unit-tested
  but not run in a match; the green patches are traced to compute program 68a84477c245d545, a
  blur that divides by a weight sum it reads as near zero. See the progress report's last section
  for the next capture to take.
- Missing flames (and probably missing geometry): draws with a tessellation stage were rejected
  and skipped. Lichii's tessellation lowering is ported (`lower_to_maxwell.h`,
  `shader_environment.cpp`, new test `ampere_tennis_tessellation.cpp`, added to the tests'
  CMake list by hand: the build directory needs a reconfigure). No rejections on boot; not yet
  confirmed in a match.
- Still open: frame rate in heavy scenes (the guest waits about 11 ms a frame on the present
  fence even at 60 FPS; whether the host GPU is the limit needs a run with `force_max_clock`
  off, since its boost shader counts as GPU load), and the occasional device loss, which followed
  the volume blits that this fix reroutes and may or may not be gone.
- A private GitHub repository exists for this work: `Mcm190/Eglantine-duskbloods` (remote
  `private`). Nothing has been pushed to it.

## Current result and the owner's latest correction

`build/bin/eglantine` now boots the private-server network test, passes Continue, and reaches
controllable gameplay. The missing Duskbloods DLSS replacement was adapted from the working
Lichii branch. Shader instruction gaps and the BCAT regulation-file failure were also fixed.

The scene blur was traced to rejected depth: the game supplies `S8Z24`, whereas the replacement
accepted only `Z32` / `D32_FLOAT`. After accepting the actual depth format, the run explicitly
logged `DLSS replaced by FSR 3.1: 1108x624 -> 1920x1080`. The owner said:

> better, certainly not blurry anymore in that run, but still a bit jittery

**That defect is fixed (later the same day): the motion vectors were handed to the upscaler about
a thousand times too large.** The owner's description was:

> no juddery, flickery, liek the scene has taken acid. walls moving a buit even when character still

and after the fix, playing the fixed build: "yup tyhat fixed it".

Cause. The motion image the replacement reads (prepass store at cbuf0+0x290) is not in UV units
for this game. The prepass multiplies the game's raw velocity target (cbuf0+0x284) by the game's
own motion scale (cbuf0+0x198/0x19c = -1080, -1080), which leaves offsets to the previous position
in output pixels. The code treated them as UV offsets with sign -1 (carried over from Lichii, where
this game never reached FSR because its `S8Z24` depth was refused). Idle sway of the character and
of loose debris, and sub-pixel camera noise, became shifts of tens to thousands of pixels, so FSR
dragged history across the scene. Fix: `motion_scale = {1 / output width, 1 / output height}` for
this title in `UpscaleInsteadOfNativeProgram` (`kepler_compute.cpp`). Nothing else changed.

Ruled out on the way, with measurements: jitter sign and units (unchanged, Halton in render
pixels), depth convention (the game clears depth to 0 and tests GEQUAL, so FSR's reversed-depth
assumption holds), the render region, and the prepass constants. Evidence, scripts and the table of
measurements: `build/bin/duskbloods-evidence/mcp-2026-11-11/motion-scale/README.md`.

A ten-second sample in the depth-fixed build held 60.0 FPS, median 16.9 ms, p95 20.0, p99 20.1,
worst 21.1. Different scenes can be slower. No universal 60 FPS claim.

GUI binary with the motion fix, SHA256: `05200d4474581bbeb4ca904a818fa5fe273dc187ad7eaa7f1f9f3ad5087ac937`
(the earlier handoff binary, `66b39521...`, is kept at `/tmp/duskbloods-mcp-work/eglantine-handoff`).

## Runtime state at handoff

- No owned emulator or private-server harness remains running. The latest owned `vrr-fifo`
  run stopped cleanly (exit 0, 186.1 s including cleanup).
- Primary display originally 1920x1080 @ 100 Hz, VRR Never. It was temporarily set to VRR Always
  for a FIFO test, then **restored to Never**. See `/tmp/duskbloods-mcp-work/display-restored.json`.
  No resolution, refresh rate, scaling or other output setting was changed.
- Native config was not modified. SHA256 of `~/.config/eglantine/qt-config.ini`:
  `3e14c132363f060f6ba55ca75f3aad9af0756b4eca9e7651932a081353f2f1e9`.
- All test settings were in copied per-run config directories. Immediate presentation was
  restored live before closing the tests; the final `vrr-fifo` copy was initially FIFO.
- The owner played the diagnostic runs. Avoid injecting walking/camera inputs while they are
  playing. Never stop an emulator they launched themselves.

## Files and provenance

Repo `/home/mcm/GitStuff/Egalatine/Eglantine`, branch `duskbloods`, base HEAD
`87ed20643a7677dbde5ad86df6abe8257f619543`. The checkout was already dirty with built-in MCP
integration from `13d34459cd29b8e61284452c65e783ea0bc33fc5`, untracked MCP files and `.rej`
artifacts. Preserve them. `src/video_core/nx2_profile.h`, various memory-layout helpers/tests
and other dirty files were also pre-existing; do not attribute all of `git diff` to this work.

Working comparison: `/home/mcm/GitStuff/Lichii`, local branch `duskbloods` at
`0143f6dd435d32a45c541341447924cb5e051f9e`. Its checked-out branch is `nx2`, so inspect with
`git -C /home/mcm/GitStuff/Lichii show duskbloods:PATH`. Do not switch or edit that checkout.

Implemented in this session:

- `delivery_cache_file_service.cpp`: validate BCAT filename with `VerifyNameValidFile` so
  `regulation_nt_1_0.bin` is accepted; private login verified.
- `nx2_gsv.h`: Duskbloods current-present fence policy. Earlier title command corruption cleared
  in these runs; environment override retained.
- `lower_to_maxwell.h`: register-handle SULD/SUST, uniform-addressed local spills and operand
  coverage, captured U64 left SHF. Narrow regression tests in `ampere_lowering.cpp`.
- `kepler_compute.cpp`: capture known Dusk prepass, preserve it, skip identified network
  internals, replace final output before its faulting native software-tensor dispatch, accept
  rendered regions inside full targets, reverse Dusk motion scale, accept S8Z24 depth.
- `duskbloods_upscale.h`: checked inclusive region bounds. Tests in `ocarina_upscale.cpp`.
- Optional region passed through rasterizer interfaces and texture cache; temporal stand-in
  uses that region in shader push constants and colour/motion pixel grid.
- `texture_cache.h`: native depth descriptor normalisation; accept depth/stencil image formats
  and construct the view in its actual format. Vulkan `DepthView()` samples the depth aspect.
- `vk_turbo_mode.cpp`: integrate pre-existing turbo pacer helper/test, bound idle boost work,
  transfer-to-compute barrier and stop on failed fence. Not the sole fix for first-scene loss.
- Catalogue `tools/nx2/catalog/0400D1D02399A000.json`: prepass e5efe04581720f15 and output
  50e47e31c31fc1f8, names/notes only.
- Bounded first-four diagnostics for temporal input contract and temporal refusal remain in
  source. Completion log distinguishes temporal / bilinear copy. Earlier `plain copy` log
  only meant no sRGB conversion and was misinterpreted; full report corrects that inference.

## Open after the motion fix

- **About 40 FPS in the graveyard outside the first building** (owner's report; sample
  `motion-scale/perf-graveyard-40fps.json`: 40.3 FPS, median 21.1 ms, p99 35.9, emulation speed
  100%). Both emulator GPU threads were largely idle in that sample (back thread 45% in
  `split wait`, front 31% `idle` plus 15% `pending sleep`), so neither is the limit there. Not
  root-caused: the candidates are the guest's own CPU work and the front's fence waits
  (136 of 153 pending waits per second ended on the timeout rather than a wake). Earlier, lighter
  scenes held 60. The upscale's cost did not change with the fix.
- Unverified: handheld mode (the game's scale is presumably minus that output's height; the fix
  divides by the output size, so it should follow), resolution scaling above 1x, fast motion and
  combat. The pan measurement was one horizontal pan in one scene.
- The temporal stand-in (no depth, or `EDEN_NX2_DLSS_REPLACEMENT=standin`) takes the same scale and
  was not re-measured.
- No unit test covers the scale: it is one expression at the call site.
- The live-probe method is worth reusing: gate an experiment on `Settings::GetDebugKnobAt(n)`, flip
  it over MCP `debug_knobs`, score bursts of `screenshot_raw` (`motion-scale/matrix.py`,
  `pansharp.py`). The old `live-jitter-probes.patch` is superseded; no probe is left in the source.
- Harness notes: the intro cutscene plays for roughly two and a half minutes after Continue in
  some runs and does not use the DLSS path; `run.py` refuses to start while an earlier run still
  holds port 18443; `EDEN_NX2_SPLIT=0` did not reach gameplay within 700 s.

## Build and tests

Normal requested build: `ninja -C build -j12 yuzu eden-cli tests`.
Ninja repeatedly reports a truncated `.ninja_deps` and schedules ~650 recompiles, so isolated
changes were compiled from `build/compile_commands.json` and then normal generated archive/link
commands were used. Important: texture-cache template implementation is explicitly instantiated
in **`src/video_core/renderer_vulkan/vk_texture_cache_base.cpp`**, not `vk_rasterizer.cpp`.
Do not edit a file while its compiler is running.

Exact isolated build pattern (Python, from repo root):

```python
import json, shlex, subprocess
from pathlib import Path
root = Path.cwd()
entries = json.loads((root / 'build/compile_commands.json').read_text())
for suffix in ['/video_core/engines/kepler_compute.cpp']:
    entry = next(e for e in entries if e['file'].endswith(suffix))
    subprocess.run(shlex.split(entry['command']), cwd=entry['directory'], check=True)
for target in ['src/video_core/libvideo_core.a', 'bin/eglantine', 'bin/eden-cli', 'bin/tests']:
    command = subprocess.check_output(['ninja', '-t', 'commands', target],
                                     cwd=root/'build', text=True).splitlines()[-1]
    subprocess.run(command, cwd=root/'build', shell=True, check=True)
```

Validation of the depth-fixed build:

- Focused `[duskbloods],[turbo_pacer],[mcp],[ocarina_upscale]`: 23 cases / 2,790 assertions pass.
- Full: 396 cases, 391 pass, same 5 pre-existing failing cases / 14 assertions in
  `ampere_logged_numeric.cpp` (before/after evidence retained). No new test failures claimed.
- Offline captured shader corpus: 207/231 before -> 231/231 with tensor option; all 207
  previously successful lowerings have identical hashes. Not all runtime stages supported.
- `git diff --check` passes. GUI, CLI and tests linked.
- Experimental jitter probes are excluded from handoff binaries.

## Runtime harness / MCP

Use existing local private servers only. Helpers are `/tmp/duskbloods-mcp-work/{run,call,navigate}.py`.
The launcher needs its venv (system Python lacks grpc/cryptography):

```sh
/home/mcm/.local/share/duskbloods-launcher/venv/bin/python -I \
  /tmp/duskbloods-mcp-work/run.py --name NEW_UNIQUE_NAME --seconds 240 --mcp
python3 -I /tmp/duskbloods-mcp-work/navigate.py /tmp/duskbloods-mcp-work/NEW_UNIQUE_NAME
```

Run.py starts visible GUI, copied settings/NAND/save, local stub/TLS front door, and stops its
own emulator/servers after its wall-clock bound. To end early, touch `<run-dir>/stop`.
Navigation sends A through the title; it often finishes before login. Inspect `screenshot_raw`
and press A once the ONLINE Continue menu actually appears. Do not blindly send gameplay input.
Game path is the NUP under `/run/media/mcm/Games/Emulators/Egalatine(Switch2)/Roms/`; old extracted
exefs paths no longer exist. Parent environment has EDEN_NX2_SPLIT=2 and FENCE_BATCH=1.

```sh
python3 -I /tmp/duskbloods-mcp-work/call.py status
python3 -I /tmp/duskbloods-mcp-work/call.py screenshot_raw --image /tmp/check.png
python3 -I /tmp/duskbloods-mcp-work/call.py press_buttons '{"buttons":["A"],"hold_ms":300}'
python3 -I /tmp/duskbloods-mcp-work/call.py perf_sample '{"seconds":10,"profile":true,"screenshot":false}' --out /tmp/perf.json
python3 -I /tmp/duskbloods-mcp-work/call.py debug_knobs '{"value":1}'
python3 -I /tmp/duskbloods-mcp-work/call.py release_all
```

MCP HTTP `127.0.0.1:6544/mcp`; legacy debug TCP6545. Standard `screenshot` was black, so use
`screenshot_raw`. Input button names uppercase (PLUS, not Plus). `get_controller_state` reports
actual game-visible buttons/sticks. `settings_set` changes live settings, per-run config only.

Live log is `~/.local/share/eglantine/log/eden_log.txt`; each stopped run copies it to its
`emulator.log`. Server logs include private session data; don't copy those into source.

## Known unresolved limitations

- Scene warping/flicker: fixed, see the top. Frame rate in heavy scenes is the next priority.
- One earlier stand-in run lost the Vulkan device at 253.4 seconds after Fermi 2D source-depth
  assertions. Depth-fixed runs ended cleanly at 229 and 186 seconds; this does not prove the
  later fault solved. Earlier 300-second gameplay run also succeeded. Keep stability separate.
- Native tessellation stage 2 still rejected / corresponding draw skipped; not fixed here.
- Public online/multiplayer compatibility is not established by local server login.
- Handheld mode, higher resolution scale and long play sessions unverified.

Full evidence is `build/bin/duskbloods-evidence/mcp-2026-11-11/`, including selected worktree patch,
source additions, binary metadata, tests, logs, screenshots and run results. No private server
certificates or full native game shaders are in that curated directory. Raw driver captures stay
outside source under `/home/mcm/radv_dumps_*`. Full progress report is in `tools/nx2/`, with local
copies in `build/bin/` and `docs/developer/` (docs is an uninitialised submodule, so the tools copy
is the source-review file).

Follow root AGENTS and the two used `.claude/skills/nx2-{lab-automation,shader-triage}/SKILL.md`.
Referenced docs/policies AI/Coding files were absent. Never push or submit a PR. Permissions
are unrestricted and approval never; do not supply sandbox_permissions. User has asked not to
prompt and authorised autonomous performance work, but now explicitly asks to hand off.
