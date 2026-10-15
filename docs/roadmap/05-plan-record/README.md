# Plan record — P00 to P07 (2026-10-09 to 2026-10-13)

The phased plan that built the editor, as it ran, and what each task left behind. The plan
files (`plan/`) and the per-task handoffs (`HANDOFF_<phase>_TASK<n>.md`, at the repository
root) existed while a plan runner executed them, one fresh session per task. They were
digested into this folder on 2026-10-14 and deleted. The mechanisms they describe are in
`../../design/`, the usage in `../../command_reference/`, the GTX numbers in
`../02-measurements.md`, the verdict on the spikes in `../03-go-no-go.md`. Every entry is
dated and names its commit; nothing is restated from memory.

## How the plan ran

- **One session per task** (`Tn` of phase `Pnn`), in the order of the table below, each
  ending in a commit and a handoff. Every session read `CLAUDE.md` and the DualC files its
  task named, ran `python scripts/check.py` green before committing, routed its facts to
  `docs/`, and never edited DualC (rule 5; a needed DualC change became a question,
  `../06-dualc-questions.md`).
- **Gates.** Most tasks ran automatically. P01, P03, P04 and P05 waited on an open decision
  (E-07, E-08, E-09, E-10 in `../../decisions/02-open.md`); P02 waited on the owner's
  approval. On 2026-10-13 the owner delegated E-08, E-09 and E-10 to the plan's
  recommendations and asked the run to continue without stopping (commit ea48aa9).
- **GTX checks** (E-04). Through P07 the owner ran each unit's check on the laptop before
  the next unit. From P03 on they were queued, not run: the unit wrote a *GTX check owed*
  section, and `../04-gtx-batch-2026-10-13.md` gathers the queue as one laptop session.
  Window checks "on any machine" that no test covers are listed in the same page.
- **Runner settings worth keeping**, from the manifest: owner Giacomo; decisions file
  `docs/decisions/02-open.md`; CI command `python scripts/check.py`; a unit never pushes;
  stop keywords `deferred`, `conditional`, `not started until`, `awaiting a decision`. The
  manifest was the owner's to change; P07's units were added by P02 T1 on the owner's
  instruction. The runner's state folder `.plan-run/` is git-ignored; it holds the owner's
  GTX bundles cited in the phase files (`P07_T1_GTX_2026-10-13.zip`,
  `P07_T1_GTX_2026-10-13_6f3a8bd.zip`, `P07_T2_GTX_2026-10-13_4b89008.zip`,
  `P07_T3_GTX_2026-10-13_b163141.zip`), whose numbers are all in `../02-measurements.md`.
- **DualC pin** throughout: `2fcb6ee` (`CMakeLists.txt`).
- **Development box** throughout: Quadro FX 3700 on nouveau (`NV92`), Mesa 26.0.8,
  Qt 6.10.2, Ubuntu, X11, Release builds. "llvmpipe" means `LIBGL_ALWAYS_SOFTWARE=1`
  (LLVM 21.1.8). Its numbers only show that tooling works; they are never performance
  figures (E-04).

## Phases, in run order

| File | Phase | Gate | Closed |
|---|---|---|---|
| `01-p00-viewport.md` | P00 — Spike A: DualC's tracer in a Qt viewport | auto; owner check at T5 | 2026-10-12 |
| `02-p01-scene-and-picking.md` | P01 — Spike B: several objects, show/hide, picking | E-07 | 2026-10-12 |
| `03-p02-go-no-go.md` | P02 — measured go/no-go | owner approval | 2026-10-13 |
| `04-p07-render-scaling.md` | P07 — culling (E-23) and asynchronous compile (E-21), added by P02 | auto; owner check at T3 | 2026-10-13 |
| `05-p03-scene-model.md` | P03 — scene model, undo, `.dcscene` | E-08 | 2026-10-13 |
| `06-p04-import.md` | P04 — mesh import, watertight solids | E-09 | 2026-10-13 |
| `07-p05-operations.md` | P05 — booleans, fills, operation panels | E-10 | 2026-10-13 |
| `08-p06-export.md` | P06 — export | auto | 2026-10-13 |

Each file holds, per task: the task as planned with its acceptance criteria (frozen), then
a dated **DONE** entry with what was delivered and how it was verified, what went wrong or
was rejected on the way (with why), the development-box numbers that show the tooling
works, what stayed open, and the owner checks, marked done or owed. ctest counts are the
suite's size at that commit (7 after P00 T4, 20 after P06).

## Files

- `01-p00-viewport.md` — P00: the render path port, the orbit camera and adaptive preview, opening and watching a graph, the Parameters dock and timing, `--bench` and the owner's first GTX measurement.
- `02-p01-scene-and-picking.md` — P01: several objects in one shader, the Objects dock, the CPU pick and highlight, the owner's object-count measurement that triggered E-23.
- `03-p02-go-no-go.md` — P02: the go verdict, the owner's answers, the plan edits applied (P07 added, E-28 recorded).
- `04-p07-render-scaling.md` — P07: per-object culling (option C, costly objects only), the asynchronous compile, the owner's cold-cache re-measurement and the open cold-compile question.
- `05-p03-scene-model.md` — P03: the undo stack, the `.dcscene` zip, the transform gizmo with a uniform scale.
- `06-p04-import.md` — P04: the OBJ, PLY and STL readers with the mesh library, import as `mesh` or `winding`, Make Watertight.
- `07-p05-operations.md` — P05: booleans with the E-10 wrap, TPMS and lattice fills with a skin, panels generated from DualC's registries.
- `08-p06-export.md` — P06: the export dialog, estimate, tiled writers (E-27) and the export job.

---
← Back to the [Roadmap index](../README.md).
