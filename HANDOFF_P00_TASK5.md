# Handoff — P00 T5: measure on the GTX 1050 Ti (human check)

Date: 2026-10-10. Plan: `plan/10_P00_viewport.md` § T5. DualC pin: `2fcb6ee` (unchanged).

**Status: blocked on the owner.** The tooling and the checklist are in. The measurements
themselves need the GTX 1050 Ti laptop (E-04), so T5's acceptance criteria and P00's exit
criteria are not met yet. See *Exit criteria* below.

## What changed and why

- **`--bench`** (`src/bench.{h,cpp}`, new, and `src/main.cpp`). It is a scripted
  measurement run, so the owner's numbers are repeatable and do not depend on hand
  orbiting. It shows a bare `Viewport` traced at `--bench-size` (default 1280×800 device
  pixels), then runs two phases of `--bench-frames` frames (default 60, after 3 warm-up
  frames):
  - *moving*: one orbit step per presented frame, so the preview is on as under a drag;
  - *settled*: full-resolution frames of a still camera.

  It prints one JSON line with, per phase, the median `GL_TIME_ELAPSED` ms and the median
  wall ms between `frameSwapped` signals. It also gives `compileMs`, `firstFrameMs`, the
  traced sizes, `GL_RENDERER` and `GL_VERSION` (on NVIDIA this carries the driver version).
  Vsync is off for the run (swap interval 0 on the default format), so the wall time is not
  rounded to the refresh. Exit codes: 1 for a link failure or a phase over 60 s, 2 when the
  graph does not load, 77 with no GL. On Linux with no display it selects `offscreen` and
  exits 77, as `--snapshot` does.
- **`Viewport::orbitByPixels(dx, dy)`**. A scripted drag through the same path as the
  mouse. Also `glRenderer()` and `glVersion()`.
- **`scripts/measure_viewport.py`** (new). It runs `--bench` over the three T5 scenes at
  0.35, 0.5 and 1.0, prints rows for the `02-measurements.md` table and appends the raw JSON
  to `measurements-<date>.jsonl`, which is now git-ignored. `--prime` sets the Linux PRIME
  offload variables. `--field-view PATH` also renders each scene through the editor's and
  dualc_field_view's `--snapshot` at 800×600 and prints the pixel diff. The PNG decoder is
  stdlib only, so the owner needs no extra package. The scenes:
  - DualC's `gyroid_box.json`;
  - the tapered octet of recipe P12 in DualC's `docs/command_reference/12-dualc_field_view/`,
    `intersection(box(...),octet(wavelength=0.5,radius=0.03,nodeRadius=0.08))`;
  - `mesh_lattice.json`. It names `cube.obj`, which DualC only generates at build time
    (`dualc_gen_demo`), so the script writes the same cube (`makeCube`, side 1, same vertex
    order and triangles) beside a copy of the graph.
- **ctest `bench`**: a 160×100, 4-frame run on the gyroid. A regex checks that both phases
  have GPU samples and the right traced sizes (80×50 and 160×100). It skips with exit 77.
- **Docs.**
  - `docs/roadmap/02-measurements.md`: a second table, *Against dualc_field_view*, and the
    six-step owner checklist.
  - `docs/command_reference/01-viewport.md`: the three flags and a *Measurement run*
    section.
  - `docs/design/01-render-path.md`: a *Measurement run* section.
  - `docs/roadmap/README.md`: current focus.
  - `plan/10_P00_viewport.md`: T5 notes. "1280×800" is the traced size. The dualc_field_view
    comparison runs at 1100×800, the size of its window. And the `cube.obj` gap.

## Measured (development box, not the reference GPU)

These numbers only show that the tools work. They are not performance figures (E-04).

- The full script on llvmpipe (`LIBGL_ALWAYS_SOFTWARE=1`, Mesa 26.0.8, LLVM 21.1.8) at
  `--size 160x100 --frames 4`: all nine runs completed, and it exited 0. Settled GPU ms:
  gyroid 21, tapered octet 135–145, mesh lattice 59–60. Compile: tapered octet 7.2 s the
  first time ever, then 0.94–0.98 s from Mesa's disk shader cache.
- `--bench` on NV92 (Quadro FX 3700, nouveau), gyroid at 640×400, 20 frames, scale 0.5:
  moving 17.0 GPU / 19.8 wall ms at 320×200, settled 48.9 GPU / 51.8 wall ms, compile 79 ms.
- The tapered octet **does not run on NV92**. A 320×200 bench at 0.35 was still in its
  first frames after 2.5 minutes, and I killed it. The phase limit cannot fire while the
  GPU is stuck in a frame.
- PNG diff self-check: two editor snapshots of the gyroid gave 0 px. NV92 against
  llvmpipe gave 31 891 / 480 000 px, max 198. T1 measured 31 878 for dualc_field_view
  across the same two drivers, so the decoder and the diff agree with T1's tool.
- `python scripts/check.py`: green, 8/8 ctests.

## What I got wrong on the way

- My first wording of `firstFrameMs` in the command reference said llvmpipe takes "over
  30 s" to the first octet frame. I had not measured that. It was a whole-process time
  with a cold shader cache, so I removed the number.
- The first version filed GPU times by a broken counter expression, and lost the last
  settled sample to a fixed 100 ms wait. Warm-up frames are now dropped by count. The run
  waits for every expected GPU time, or for 1 s.

## Exit criteria of P00

- **T1–T4 met**: yes, see `HANDOFF_P00_TASK1.md` to `HANDOFF_P00_TASK4.md`. The owner
  checked T2's camera live.
- **T5 met**: **no.** `02-measurements.md` has no GTX rows yet, and the dualc_field_view
  match is not stated. Both need the owner's run of the checklist.
- **`python scripts/check.py` green**: yes, 8/8 ctests on the development box.
- **`docs/roadmap/README.md` § Current focus names P01**: **not yet.** It says that P00
  closes when the GTX numbers are in. Naming P01 before T5 is met would misstate the
  status.

## Open

- **The owner's run** (needsHumanCheck): `docs/roadmap/02-measurements.md` § How to measure,
  steps 1–6. Then paste back:
  - the script's nine rows and its `GPU:` line;
  - the `--field-view` diff lines;
  - the dualc_field_view fps against the editor's `--bench --bench-size 1100x800` wall ms;
  - what the live window looks like, including whether T2's blue-red-green squares on the
    silhouette during a drag show on the GTX.
- **Comparing frame times with dualc_field_view is indirect.** It prints no timing, so its
  side is read from the NVIDIA overlay or PresentMon with vsync off. The editor's matching
  number is the wall ms, not the GPU ms. Proper timing in dualc_field_view would be a DualC
  change (rule 5).
- `--bench` is untested on Windows and on HiDPI. The size is set in logical pixels as
  `size / devicePixelRatio`, and the JSON reports the size actually traced.

## For the next unit

- Once the numbers are back: fill both tables in `02-measurements.md`, state the
  dualc_field_view match in this handoff, switch `docs/roadmap/README.md` § Current focus
  to P01, and log any finding: a mismatch, the preview artefacts.
- P01 T4 (frame time against object count) can reuse `--bench` and the script.
