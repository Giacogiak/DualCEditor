# Handoff — P00 T5: measure on the GTX 1050 Ti (human check)

Date: 2026-10-10, closed 2026-10-12. Plan: `plan/10_P00_viewport.md` § T5. DualC pin: `2fcb6ee` (unchanged).

**Status: done.** The owner ran the checklist on the GTX 1050 Ti (Windows 10, driver
441.93, commit 53c6cb6). All nine rows are in `docs/roadmap/02-measurements.md`. Three rows
of the pasted report were garbled. Their cells were taken from the owner's raw JSON lines,
sent on 2026-10-12, not guessed. The raw file of both runs is committed as
`docs/roadmap/02-measurements-p00.jsonl`.

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

## Owner's results (2026-10-10, second session)

Conditions: Windows 10, GeForce GTX 1050 Ti, driver 441.93 (GL 3.3.0), Qt 6.8.3
msvc2022_64 (aqtinstall), MSVC 14.44, Ninja, Release, commit 53c6cb6. ctest 8/8 passed on
the laptop. Display scaling 100%, mains power, Balanced plan. The tables are in
`docs/roadmap/02-measurements.md`. In brief:
- 1280×800, settled GPU / wall ms: gyroid 2.6–2.7 / 4.2–5.3; mesh_lattice 7.3 / 8.2–8.3;
  tapered octet 376–390 / 361–390. Moving at 0.35: gyroid 0.8 / 3.8, mesh_lattice
  1.9 / 3.9, octet 80.6 / 83.3. Compile 19–30 ms with a warm shader cache. The cold
  compiles of the first run were 945.4 ms (mesh_lattice) and 327.1 ms (octet).
- Garbled in the pasted report, then re-read from the raw JSON: gyroid 1.0 (moving
  3.36 GPU / 4.84 wall, settled 2.59 / 4.08), mesh_lattice 0.5 (moving 2.74 / 3.80,
  settled 7.33 / 8.25), and tapered octet 1.0, confirmed as pasted (moving 402.01 / 395.74,
  settled 389.76 / 360.88). The octet's settled wall median is under its GPU median
  because the two are medians of different samples: 52 GPU times, 60 wall times.
- Live window: gyroid and mesh_lattice are immediate. The tapered octet lags, mostly at
  the start of a drag. No coloured squares on any scene, so T2's artefact is a nouveau and
  Quadro matter.

### Does the viewport match dualc_field_view on the same laptop?

- **Image: yes.** 800×600 snapshots differ in 111 px (gyroid), 44 px (octet) and 715 px
  (mesh_lattice) out of 480 000, by at most 1, 1 and 20 per channel. The owner saw no
  visible difference on mesh_lattice.
- **Frame time: within about 10% where the comparison is like for like, with one
  exception at small absolute cost.** The comparison is dualc_field_view's drag frame time
  (PresentMon median) against the editor's `--bench --bench-size 1100x800`
  `moving.wallMs`:
  - within 10%: gyroid 0.35 (+10%), octet 0.35 (+6%), mesh_lattice 0.35 (−2%);
  - over 10%: gyroid 1.0 and mesh_lattice 1.0, both +15% (+0.6 ms and +1.2 ms per frame),
    likely a fixed per-frame composition cost of QOpenGLWidget;
  - not like for like: octet 1.0 (272 against 393 ms, the editor faster). The viewer's
    window size was not checked, and its 393 ms matches the editor's 1280×800 times.

  Settled frames cannot be compared: dualc_field_view only draws back to back while the
  view moves.

## What I got wrong on the way

- **The first checklist said dualc_field_view draws continuously.** It does not: when
  settled, it waits up to 0.1 s for input before each redraw. I read its main loop's
  drawing half and missed the wait policy above it. The owner caught it. Their "still"
  numbers (for example 33.1 ms for the gyroid at 1.0) are waits, not traces. Step 5 of the
  checklist now compares only the drag frame time with `moving.wallMs`. On Windows it names
  PresentMon (admin shell), since the Alt+R overlay needs GeForce Experience 3.20 or later.
- The checklist did not say that the Windows build does not deploy Qt. The owner ran
  `windeployqt`. Step 1 now says so.

- My first wording of `firstFrameMs` in the command reference said llvmpipe takes "over
  30 s" to the first octet frame. I had not measured that. It was a whole-process time
  with a cold shader cache, so I removed the number.
- The first version filed GPU times by a broken counter expression, and lost the last
  settled sample to a fixed 100 ms wait. Warm-up frames are now dropped by count. The run
  waits for every expected GPU time, or for 1 s.

## Exit criteria of P00

- **T1 to T4 met**: yes, see `HANDOFF_P00_TASK1.md` to `HANDOFF_P00_TASK4.md`. The owner
  checked T2's camera live.
- **T5 met**: yes.
  - `docs/roadmap/02-measurements.md` has one row per scene and scale: 3 × 3, each with
    the moving and settled ms (GPU query and wall), the compile ms, and the commit, OS and
    driver. The conditions are under the table.
  - Whether the viewport matches `dualc_field_view` is stated above: the image does; the
    frame time is within about 10% on the like-for-like rows, with a +0.6 to +1.2 ms
    per-frame cost on light scenes at 1.0.
- **`python scripts/check.py` green**: yes, 8/8 ctests on the development box. On the
  laptop, ctest 8/8 passed.
- **`docs/roadmap/README.md` § Current focus names P01**: yes.

## Open

- **The status bar's settled `frame` reads 7–12× high on light scenes** in the live window
  (30 against 2.6 ms for the gyroid), while the preview agrees. The owner's hypothesis is
  that the GPU drops its clocks once the drag ends, before the one settled frame. It is not
  verified. To test it, time a full frame after an idle gap in `--bench`. It does not block
  P01. The status bar is diagnostic, and the bench numbers are the measurements.
- **The light-scene composition cost** (+0.6 to +1.2 ms per frame at 1.0) against
  dualc_field_view. It is small in absolute terms. It is worth a look if P01 wants many
  objects at high frame rates: drawing to the window directly, without QOpenGLWidget's
  framebuffer, would avoid it.
- **The tapered octet 1.0 comparison** needs a re-run with the viewer's window checked at
  1100×800.
- **The tapered octet is not interactive on the reference GPU** (8–12 fps at 0.35–0.5).
  dualc_field_view is no faster. This is input for P02's go/no-go.
- `--bench` on HiDPI is still untested. It ran on Windows at 100% scaling.

## For the next unit

- P00 is closed. P01 waits on E-07 (picking mechanism), which is a stop unit in the
  manifest.
- P01 T4 (frame time against object count) can reuse `--bench` and
  `scripts/measure_viewport.py`. Compare `moving.wallMs` only with a viewer's drag frame
  time, and fix both window sizes.
- On Windows, deploy Qt with `windeployqt` before running the binaries.
