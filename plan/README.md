# Plan

The phased work `/plan-run` executes: one fresh session per task (`### Tn.`), in filename
order, each ending with a committed `HANDOFF_<phase>_TASK<n>.md`. Background and
evidence: `../docs/roadmap/01-feasibility.md`. Decisions a phase waits on:
`../docs/decisions/02-open.md` (table in `../docs/decisions/README.md`).

Every unit:
- reads `CLAUDE.md`, then the DualC files its task names (DualC is fetched into
  `build/_deps/dualc-src/`, or is the tree named by `DUALCEDITOR_DUALC_DIR`);
- runs `python scripts/check.py` green before committing;
- routes every fact it produces to its owner under `docs/` (the table in `docs/README.md`);
- never edits DualC; a DualC change is written up as a question for the owner.

Tasks marked **human check** need the owner on the GTX 1050 Ti laptop (E-04); this
development box cannot measure performance.

## Phases

| File | Phase | Gate |
|---|---|---|
| `10_P00_viewport.md` | Spike A: DualC's tracer in a Qt viewport | auto, human check at T5 |
| `20_P01_scene_and_picking.md` | Spike B: several objects, show/hide, picking | stop on E-07 |
| `30_P02_go_no_go.md` | Measured go/no-go before the full editor | approve |
| `40_P03_scene_model.md` | Scene model, undo, `.dcscene` | stop on E-08 |
| `50_P04_import.md` | Drag-and-drop mesh import, watertight solids | stop on E-09 |
| `60_P05_operations.md` | Booleans, lattice fills, operation panels | stop on E-10 |
| `70_P06_export.md` | Mesh extraction and export with progress | auto |

## Other files

- `run.manifest.json` — the `/plan-run` driver's run order and gates, one unit per task. Giacomo's to change, not a unit's.
