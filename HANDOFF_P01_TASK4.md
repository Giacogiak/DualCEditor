# Handoff — P01 T4: frame time against object count (human check)

Date: 2026-10-12 (two sessions). Plan: `plan/20_P01_scene_and_picking.md` § T4. DualC pin: `2fcb6ee` (unchanged).

**Status: done.** The owner ran `scripts/measure_viewport.py --objects` on the GTX 1050 Ti
(Windows 10, driver 441.93, commit b011b24). The twelve rows and the growth per added object
are in `docs/roadmap/02-measurements.md`, and the raw JSON is
`docs/roadmap/02-measurements-p01.jsonl`. **E-23 is triggered.** P01 is closed.

## What changed and why

- **`scripts/measure_viewport.py --objects`.** The owner's numbers should be repeatable and
  not depend on hand-made scene files, as in P00 T5. With `--objects` the script writes one
  `.fld` per object and runs `DualCEditor --bench` over six scenes, at preview scales 0.35
  and 1.0, traced at 1280×800, 60 frames per phase:
  - 1, 3, 5 and 10 analytic primitives: a fixed 5×2 grid of slots 1.2 apart, alternately a
    sphere of radius 0.4 and a box of side 0.8. None overlap. The view frames the union,
    so the screen share the objects cover stays about the same across counts;
  - 1 and 3 TPMS-in-box: `gyroid_box.json` without its spherical hole, translated 5 apart
    along x.

  It prints rows for the first table of `docs/roadmap/02-measurements.md` (unit `P01.4`),
  then four *per added object* lines (primitives and TPMS-in-box, at each scale): the
  least-squares slope of each median (moving and settled, GPU and wall) against the object
  count. The raw JSON lines carry `scene` and `objects`.
  - `run_bench` now takes a list of graphs. `row` takes the unit.
  - `--keep` prints the scratch folder, so the owner can open the scenes in the window.
  - Without `--objects` the P00 run is unchanged.
- **Docs.**
  - `docs/roadmap/02-measurements.md`: the twelve P01.4 rows in the first table, and
    § Against object count (P01 T4) with the conditions, the scenes, the growth table,
    the findings, the llvmpipe tool check and a three-step checklist.
  - `docs/roadmap/02-measurements-p01.jsonl` (new, indexed): the owner's raw JSON lines.
  - `docs/decisions/03-deferred.md` § E-23: the owner's threshold (any clear slope,
    committed by the owner as 8f23e06) and the line that it fired.
  - `docs/command_reference/01-viewport.md` § Measurement run: `--objects`.
  - `docs/roadmap/README.md`: current focus is now P02. P01 is under *Done*.
  - `plan/20_P01_scene_and_picking.md`: T4 notes.

No C++ changed. The ctests are the T3 set.

## Measured on the reference GPU (owner, 2026-10-12)

Conditions: the owner's laptop, Windows 10 (10.0.19045), GeForce GTX 1050 Ti/PCIe/SSE2,
driver 441.93 (GL 3.3.0), Qt 6.8.3, Release build at b011b24 (the pull to it changed no
C++), mains power, the editor closed during the run, traced 1280×800, 60 frames per phase,
nothing selected. Medians of `GL_TIME_ELAPSED` (GPU) and of the time between presented
frames (wall).

| Scene | Settled GPU / wall ms at 1.0 | Moving GPU / wall ms at 0.35 (448×280) |
|---|---|---|
| 1 primitive | 0.59 / 2.67 | 0.21 / 4.06 |
| 3 primitives | 1.06 / 2.6 | 0.3 / 3.0 |
| 5 primitives | 0.87 / 2.6 | 0.4 / 3.5 |
| 10 primitives | 3.16 / 4.1 | 1.1 / 2.9 |
| 1 TPMS-in-box | 2.64 / 3.5 | 0.8 / 2.8 |
| 3 TPMS-in-box | 7.62 / 8.6 | 2.1 / 3.0 |

**Growth per added object** (least-squares slope, settled GPU / wall ms):
- primitives (1, 3, 5, 10): **+0.28 / +0.17 at 1.0**, +0.34 / +0.12 at 0.35;
- TPMS-in-box (1, 3): **+2.49 / +2.52 at 1.0**, +2.28 / +2.29 at 0.35.

The owner's own reading: about +0.29 ms per primitive and +2.5 ms per TPMS box. The
5-primitive row is below the 3-primitive row; the owner calls it noise, on frames under
1.1 ms.

**E-23: triggered.** The owner's threshold (answer of 2026-10-12, in
`docs/decisions/03-deferred.md`) is any clear slope. Both families show one. A threshold of
0.2 ms per primitive at 1.0 would also have fired (0.28–0.37 ms GPU).

**Headroom.** The worst scene, 3 TPMS-in-box at 1.0, is 7.6–8.7 GPU / 8.6–9.4 wall ms,
under 16 ms. The moving wall time at 0.35 is 2.8–4.1 ms for every scene: the fixed
per-frame cost P00 T5 saw dominates, not the trace.

**Cold compile.** Each scene's first run (at 0.35) compiled cold: 157 / 265 / 413 / 670 ms
for 1 / 3 / 5 / 10 primitives, about +57 ms per primitive, and 339 / 1210 ms for 1 / 3
TPMS-in-box. The second run (at 1.0), from the driver cache, took 14–30 ms. This is for P02
to weigh against E-21 (asynchronous compile).

## Measured (development box, not the reference GPU)

These only show that the tooling works (E-04). llvmpipe (`LIBGL_ALWAYS_SOFTWARE=1`,
Mesa 26.0.8, LLVM 21.1.8, Qt 6.10.2), `--objects --size 160x100 --frames 4`, commit 2c9de42
plus the script change. All twelve runs exited 0.

| Scene | Settled GPU ms at 0.35 | Settled GPU ms at 1.0 |
|---|---|---|
| 1 primitive | 1.8 | 1.7 |
| 3 primitives | 4.3 | 4.2 |
| 5 primitives | 4.0 | 4.1 |
| 10 primitives | 15.3 | 15.9 |
| 1 TPMS-in-box | 22.4 | 21.3 |
| 3 TPMS-in-box | 61.5 | 61.8 |

Per added object at 1.0, settled GPU: primitives +1.57 ms, TPMS-in-box +20.2 ms. On a CPU
rasteriser this says nothing about the GTX, but the growth is clearly there: the scene
shader has no culling, so every march step evaluates every visible object's field.

`--snapshot` of the ten primitives and of the three TPMS boxes on llvmpipe at 800×600:
every object is drawn, in its own colour, and none overlap.

`python scripts/check.py`: green, 12/12 ctests pass here, none skipped.

## What I got wrong on the way

- The growth lines at 0.35 came out empty. `previewScale` in the bench JSON is the float
  0.3499999940395355, and I compared it with `==`. It now compares within 1e-3.

## Exit criteria of P01, one by one

- **T1 met** (`HANDOFF_P01_TASK1.md`): 3 objects in their colours, visibility toggles with
  links and compiles unchanged, `03-scene-lowering.md` written and indexed.
- **T2 met** (`HANDOFF_P01_TASK2.md`): two drops give two objects; show, hide, select and
  delete from the panel; command reference updated.
- **T3 met** (`HANDOFF_P01_TASK3.md`, `pick` ctest). The owner checked it on the laptop
  (2026-10-12): the front object is selected where two overlap, and a hidden object is
  skipped by a click.
- **T4 met**: the twelve rows are in `02-measurements.md` (unit P01.4), and the growth per
  added object and the E-23 call are above.
- **`python scripts/check.py` is green**: 12/12 ctests, run before the commit.
- **`docs/roadmap/README.md` names P02**: § Current focus, "Now: P02, go or no-go", and
  § Next up.

## Open

- **The primitive scenes all lie in z = 0**, so most rays meet one or two objects. A scene
  of objects stacked in depth is not measured. Culling (E-23) would help the planar case
  most.
- **No mesh objects** in these scenes. E-22's trigger (several mesh objects) is not
  measured here.
- **The cold compile grows about 57 ms per primitive** on the GTX. With 10 TPMS objects it
  could pass a few seconds. P02 weighs it against E-21.

## For the next unit (P02)

- `02-measurements.md` now holds both phases' rows. P02's note cites them, not this file.
- E-23 is triggered; P02 records the triggers in `docs/decisions/03-deferred.md` and plans
  the culling.
- `measure_viewport.py --objects` can re-measure after E-23 lands: same scenes, same
  growth lines, so the slope before and after compares directly.
