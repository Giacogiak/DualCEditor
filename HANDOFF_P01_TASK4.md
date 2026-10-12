# Handoff — P01 T4: frame time against object count (human check)

Date: 2026-10-12. Plan: `plan/20_P01_scene_and_picking.md` § T4. DualC pin: `2fcb6ee` (unchanged).

**Status: blocked on the owner's run.** The tooling is done and checked on this box. The
numbers must come from the GTX 1050 Ti (E-04), so T4's acceptance criteria and the phase's
exit criteria cannot close until Giacomo runs the checklist and sends the output back.

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
  - `docs/roadmap/02-measurements.md` § Against object count (P01 T4): the scenes, an
    empty growth table, the llvmpipe tool check, and a three-step checklist.
  - `docs/command_reference/01-viewport.md` § Measurement run: `--objects`.
  - `docs/roadmap/README.md`: current focus. It still names P02 (exit criterion).
  - `plan/20_P01_scene_and_picking.md`: T4 notes.

No C++ changed. The ctests are the T3 set.

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
- **T3 met** on the automatic checks (`HANDOFF_P01_TASK3.md`, `pick` ctest). Its own human
  look (highlight colours, click versus drag) is still asked for, below.
- **T4 not met**: the rows are not in `02-measurements.md`, and the growth per added
  object and the E-23 call need the GTX numbers. **This is the block.**
- **`python scripts/check.py` is green**: checked before the commit.
- **`docs/roadmap/README.md` names P02**: § Next up, "P02 then decides go or no-go from the
  GTX 1050 Ti measurements". Unchanged and checked.

## Open

- **The owner's run** (checklist in `02-measurements.md` § Against object count): on the
  laptop, `python scripts/measure_viewport.py --objects` (Windows) or with `--prime`
  (Linux). Paste back the `GPU: …` line, the twelve rows, the four growth lines and the raw
  JSON. The next session fills the tables, states the growth per added object here, and
  decides whether E-23 is triggered.
- **What would trigger E-23.** Its trigger is "frame time grows with the number of
  non-overlapping objects". The scenes do not overlap, so any clear slope does. I expect
  one: the union evaluates every object at every step. A slope small next to the frame
  (say, under 0.2 ms per primitive at 1.0) would argue for deferring it further; that line
  is the owner's to draw.
- The primitive scenes all lie in z = 0, so most rays meet at most one or two objects.
  Culling would help those rays the most. A scene of objects stacked in depth is not
  measured.
- No mesh objects in these scenes (E-22's trigger, several mesh objects, is not measured
  here).

## For the next unit

- When the numbers come, the 02-measurements rows can be pasted as printed. The growth
  lines go in the *Per added object* table (one row per family and scale). The raw JSON can
  be committed beside `02-measurements-p00.jsonl` as `02-measurements-p01.jsonl`, and
  indexed in `docs/roadmap/README.md`.
- P02 reads these to decide go or no-go and which of E-21, E-22, E-23 the numbers trigger.

## Human check asked

1. The T4 checklist above, on the GTX 1050 Ti.
2. T3's human look, still open (`HANDOFF_P01_TASK3.md` § Human check asked): the amber
   highlight and dimming, click versus drag feel, overlap picking.
3. Optional: open the ten primitives in the window (`--keep`) and orbit at 0.35 and 1.0.
