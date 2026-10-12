# Handoff — P02 T1: write the go/no-go note

Date: 2026-10-12. Plan: `plan/30_P02_go_no_go.md` § T1. DualC pin: `2fcb6ee` (unchanged).

**Status: written, waiting for the owner's approval** at P02's gate (`approve` in the
manifest). The note says **go**. P02's exit criterion "the owner has approved the note
before P03 starts" cannot be closed by a unit, so this unit returns `blocked` with the
approval and the choices it needs as questions.

## What changed and why

- **`docs/roadmap/03-go-no-go.md`** (new, indexed in `docs/roadmap/README.md`). It compares
  the P00.5 and P01.4 rows of `02-measurements.md` with `01-feasibility.md` § Expected
  performance. Each number is cited as *unit · scene · preview scale* of the first table,
  or by its section (§ Against dualc_field_view, § Against object count), not restated
  from handoffs. Sections: verdict, met, missed and by how much, not measured, deferred
  items triggered, changes P03–P06 need.
- **`docs/decisions/03-deferred.md`**:
  - E-21: **Triggered (P02.1)** by its second clause ("any phase from P03 onward"), with
    the cold-compile numbers, and the note's reading of what to build.
  - E-23: **Planned (P02.1)**, pointing at the note's item 1. Its trigger was recorded at
    P01.4.
  - E-22: **Not fired (P02.1)**: no several-mesh scene was measured.
- **`docs/roadmap/README.md`**: current focus (T1 written, go, waiting for approval), next
  up, the file list.
- **`plan/30_P02_go_no_go.md`**: T1 notes.

No C++, no script, no test changed.

## The numbers the verdict rests on

All from `docs/roadmap/02-measurements.md`: GTX 1050 Ti, Windows 10 (10.0.19045), driver
441.93 (GL 3.3.0), Qt 6.8.3, Release, traced 1280×800, medians of 60 frames, vsync off;
P00.5 rows at commit 53c6cb6 (2026-10-10), P01.4 rows at b011b24 (2026-10-12).

| Against the target | Row | Moving GPU / wall ms | Settled GPU / wall ms |
|---|---|---|---|
| Met | P00.5 · gyroid_box.json · 0.35 / 0.5 | 0.8 / 3.8 · 1.1 / 3.8 | 2.7 / 5.3 · 2.6 / 4.2 |
| Met | P00.5 · mesh_lattice.json · 0.35 / 0.5 | 1.9 / 3.9 · 2.7 / 3.8 | 7.3 / 8.2 · 7.3 / 8.3 |
| Met | P01.4 · 10 primitives · 1 | 3.9 / 4.7 | 3.2 / 4.1 |
| Met | P01.4 · 3 TPMS-in-box · 1 | 8.7 / 9.4 | 7.6 / 8.6 |
| **Missed** | P00.5 · tapered octet (P12) · 0.35 / 0.5 | 80.6 / 83.3 · 127.6 / 128.6 | 385.3 / 389.5 · 375.7 / 372.2 |

- "Interactive" has no number in the feasibility note. The note uses **33 ms per moving
  frame**; the octet misses it by 2.5× (0.35) and 3.9× (0.5). `dualc_field_view` takes
  73.0 ms on it at 0.35 (§ Against dualc_field_view), so it is DualC's trace cost.
- Cold compile, P01.4: 157 / 265 / 413 / 670 ms for 1 / 3 / 5 / 10 primitives, 339 /
  1210 ms for 1 / 3 TPMS-in-box; P00.5 mesh_lattice 945.4 ms. Warm (driver cache):
  14–31 ms. These run on the GUI thread.
- Growth per added object, settled GPU at 1.0: +0.28 ms per primitive, +2.49 ms per
  TPMS-in-box (§ Against object count).

Nothing was measured in this unit: it is a reading of the owner's runs. `python
scripts/check.py`: green, 12/12 ctests, on the development box.

## Plan edits the note proposes (not applied)

From `docs/roadmap/03-go-no-go.md` § Changes P03–P06 need. The owner approves each.

1. **Schedule E-23 (culling) and E-21 (async compile) before P05.**
   - (a, recommended) a new phase P07, `plan/35_P07_render_scaling.md`, run right after
     P02. Neither depends on E-08. Needs new units in `plan/run.manifest.json`, and a row
     in `plan/README.md`.
   - (b) P03 T4 (E-23) and P03 T5 (E-21), after the gizmo, waiting on E-08 with P03.

   Each task's acceptance is drafted in the note: for culling, unchanged snapshots, no
   compile on hide/show, the GTX slope re-measured well below +0.28 ms per primitive, and
   a depth-stacked family in `--objects`; for async compile, the window stays responsive
   and draws the old scene while the new one links, a ctest for the swap, and an owner
   timing.
2. **P03 T3 (gizmo)**: a moved object's trace box (and culling box) follows as uniforms;
   acceptance gains "a moved object is still drawn whole, with no compile".
3. **P04 T2**: a human check of 1, 3 and 5 imported meshes on the GTX through a
   `--meshes` family in `measure_viewport.py` — E-22's trigger, unmeasured today.
4. **P05 T2**: a human check of the octet fill at 0.35 on the GTX against
   P00.5 · tapered octet (P12) · 0.35.
5. **A new deferred item, E-28 (the next free id, not yet written into
   `03-deferred.md`): an interruptible settle** — the full-resolution frame drawn in tiles
   so a drag starting during it waits one tile, optionally with an automatic preview-scale
   drop past 33 ms. Trigger: P05 T2's octet fill still lags at the start of a drag.
6. **P06**: no change.

## What I got wrong on the way

- I first appended the new file's line at the end of the roadmap README's file list after
  `02-measurements-p00.jsonl`, out of order; moved it after `02-measurements-p01.jsonl`.
- Nothing else; no code was written.

## Exit criteria of P02, one by one

- **T1 met**, against its acceptance criteria:
  - `docs/roadmap/03-go-no-go.md` exists and is indexed (`check.py --fast` passes); it
    cites rows of `02-measurements.md` by unit, scene and scale.
  - Each fired trigger is recorded on its item: E-21 (fired now), E-23 (fired at P01.4,
    now also planned) in `docs/decisions/03-deferred.md`; E-22 marked not fired.
  - This handoff lists the six proposed plan edits; none is applied.
- **The owner has approved the note before P03 starts: open.** This is the gate. It is
  the reason this unit returns `blocked`.

## Open

- **The owner's approval**, and the choices in it: the 33 ms reading of "interactive";
  E-21's trigger read by its second clause; option 1a or 1b; items 2–5.
- The start-of-drag lag on the octet is attributed to the in-flight settle frame from the
  render path, not measured. A bench phase that starts a drag right after a settle would
  confirm it.
- The status bar's settled `frame` reading high (P00 T5) stays open; the bench numbers
  are the measurements.

## For the next unit

- If the owner approves, apply the approved edits in the plan files first (and the
  manifest for 1a is the owner's), then write E-28 into `03-deferred.md` if item 5 is
  approved, and mark the note's status line approved.
- P03 still waits on E-08 (`docs/decisions/02-open.md`).
- `measure_viewport.py --objects` re-measures the culling directly: same scenes, same
  growth lines.
