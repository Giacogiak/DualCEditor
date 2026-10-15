# Roadmap

How the editor got here and what is next. The phased plan that built it (P00 to P07) ran
from 2026-10-09 to 2026-10-13 and is recorded phase by phase in `05-plan-record/`; the
numbers it rests on are in `02-measurements.md`, its verdict on the spikes in
`03-go-no-go.md`.

## Current focus

**No phase is running (2026-10-14).** The plan is done: every phase in
`05-plan-record/README.md` has run, P06 was the last, and no phase is next until the owner
adds one. What is open:

- **The GTX checks queued by P03–P06** (E-04), one laptop session:
  `04-gtx-batch-2026-10-13.md`. It decides E-22 (several imported meshes, § 5) and E-28
  (the octet fill's lag at the start of a drag, § 7), asks for a second cold-compile sample
  and the `main_window` output (§ 9), and lists the window checks on any machine that no
  test covers (§ 10).
- **Cold shader compiles are longer than P01.4's** (P07 T3), cause unknown: 10 primitives
  1137 ms against 670, 3 TPMS-in-box 1897 against 1210, and the window's compile thread
  2567 ms for the same scene. The window stays responsive through them. A first check that
  needs no GTX: diff the 10-primitive fragment source at b011b24 and at HEAD on the
  development box (`05-plan-record/04-p07-render-scaling.md` § T3).
- **The `main_window` ctest fails one check on the GTX** (owner, 2026-10-13): "the .fld
  renders a different image" (`tests/main_window_test.cpp:220`), at 8b9baf8 and at every
  commit since, so before P07; it passes on the development box. The GTX ctest is one
  short (12/13 at 4b89008). To be investigated with the GTX in the loop. **A lead
  (2026-10-14):** the same check fails on the development box under llvmpipe
  (`LIBGL_ALWAYS_SOFTWARE=1 ctest -R main_window`), with other paint-timing checks, on a
  loaded box; seen once at 0c08ec9 plus the Parameters-dock change, not at an earlier
  commit. If it repeats on a quiet box, the failure can be chased here without the GTX.
  P07 T2 noted a candidate: the test's `quiet()` waits 500 ms with no paint, which a slow
  compile can outlast. The paint-timing family is load-sensitive on this box in general:
  on 2026-10-14 `viewport_no_preview` failed "a held, still drag repaints nothing" once
  while other work ran beside the gate, then passed alone and in a quiet full run (20/20,
  172 s); P07 T1 saw `viewport` do the same on a box just rebooted.
- **Questions for the DualC owner** (rule 5), none raised yet: `06-dualc-questions.md`.
- **The status bar's settled `frame` reads high on light scenes** (P00 T5,
  `02-measurements.md` § Findings (P00 T5)): diagnostic only, the bench is the measurement.

**Owner feedback after the plan (2026-10-14),** from a session in the live editor:
- *The Parameters dock should follow the selection like the Transform dock, one object at
  a time, instead of listing every object and scrolling.* Done the same day (commit
  95e1a8d): with an object selected the dock shows its section only, with none it shows
  all (`../command_reference/01-viewport.md` § Parameters, `objects_panel` ctest).
- *A faceted, torn mesh sphere* in the demo scene was the demo file (a bad text edit of a
  generated OBJ), not the import. Found on the way: an imported mesh file is not watched,
  by design (`../design/07-mesh-import.md`), so a mesh edited on disk needs a re-drop;
  graphs reload on save. Open, if wanted: watch mesh files too.
- *A section tool is missing: X/Y/Z toggles and −/+ buttons to move the cut.* Done the same
  day (commit 1b59389) as a toolbar and `View ▸ Section`, on the trace shader's existing
  `uClip` planes (`../command_reference/01-viewport.md` § Section; `viewport` and
  `parameter_dock` ctests). Viewing only, not saved.

## Next up

Nothing is planned. When the owner adds a phase, its record goes in `05-plan-record/` as a
new numbered file, the decisions it waits on in `../decisions/02-open.md`, and this section
names it. Candidates the record left, not scheduled and in no order: watch the mesh files
a graph names; an "unsaved changes" prompt and a dirty mark (`QUndoStack::cleanChanged`
is the hook); `featureScale` following a scale (P03 T3); a colour picker in the Objects
dock; a two-pass culling scheme (P07 T1's option B) if a later scene needs more; a
`--export` flag over `exportGraph` + `buildExportField` + `runExport` (P06).

## Phases

In run order. Each record file holds the tasks as planned, what was delivered and
verified, what went wrong, what stayed open and the owner checks.

| Phase | What the user got | Record | Status |
|---|---|---|---|
| P00 Spike A: the viewport | DualC's tracer in a `QOpenGLWidget`, orbit camera, adaptive preview, open and watch a graph, Parameters dock, status-bar timing, `--snapshot`, `--bench` | `05-plan-record/01-p00-viewport.md` | DONE 2026-10-12 |
| P01 Spike B: scene and picking | Several objects in one shader, the Objects dock, click to select with highlight, the object-count measurement | `05-plan-record/02-p01-scene-and-picking.md` | DONE 2026-10-12; E-23 triggered |
| P02 Go or no-go | The verdict on the spikes: go (`03-go-no-go.md`); P07 added, E-28 recorded | `05-plan-record/03-p02-go-no-go.md` | DONE 2026-10-13 |
| P07 Render scaling | Per-object culling of costly objects (E-23), asynchronous compile (E-21), the cold-cache re-measurement | `05-plan-record/04-p07-render-scaling.md` | DONE 2026-10-13; cold compile open |
| P03 Scene model | Undo and redo (E-25), `.dcscene` (E-08), the transform gizmo and dock with a uniform scale | `05-plan-record/05-p03-scene-model.md` | DONE 2026-10-13; GTX checks queued |
| P04 Mesh import | OBJ, PLY, STL readers (E-09), import as `mesh` or `winding`, Make Watertight | `05-plan-record/06-p04-import.md` | DONE 2026-10-13; GTX check queued (E-22) |
| P05 Operations | Booleans with the E-10 wrap, TPMS and lattice fills with a skin, panels from DualC's registries | `05-plan-record/07-p05-operations.md` | DONE 2026-10-13; GTX checks queued (E-28) |
| P06 Export | Export dialog with estimate and bounds, tiled STL and 3MF (E-27), export job with cancel | `05-plan-record/08-p06-export.md` | DONE 2026-10-13; GTX check queued |

## Files

- `01-feasibility.md` — the 2026-10-09 analysis: what DualC already provides, the blockers, the expected performance.
- `02-measurements.md` — frame and compile times measured on the reference GPU (E-04), filled by P00, P01 and P07; the owed P04 T2 table; the owner's checklists.
- `02-measurements-p00.jsonl` — the raw `--bench` output of the owner's two P00.5 runs on the GTX 1050 Ti (2026-10-10).
- `02-measurements-p01.jsonl` — the raw `--bench` output of the owner's P01.4 run on the GTX 1050 Ti (2026-10-12).
- `02-measurements-p07.jsonl` — the raw `--bench` output of the owner's GTX 1050 Ti runs of P07.1 (2026-10-13): the first 20 lines at dd92129 (culling all objects), the next 20 at 6f3a8bd (culling costly objects only, option C); and of P07.3: the last 20 at b163141, cold shader cache.
- `03-go-no-go.md` — P02's verdict from the measurements: what met the feasibility target, what missed, the deferred items triggered, and the plan edits proposed for P03–P06.
- `04-gtx-batch-2026-10-13.md` — the one laptop session that clears the GTX checks queued by P03–P06 (and two older items), plus the window checks on any machine: what to run at the newest commit, what is expected, where the answers are filed.
- `05-plan-record/` — the phased plan P00–P07 as it ran, one file per phase: tasks, acceptance, what was delivered and verified, what went wrong, what stayed open. Digested from the runner's plan and handoff files on 2026-10-14.
- `06-dualc-questions.md` — the DualC changes the editor would like (rule 5), each with its editor-side workaround; none raised yet.
