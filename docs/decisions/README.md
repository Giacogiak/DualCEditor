# Decisions

Editor decisions are `E-NN`; a DualC decision is cited as `DualC D-NN`. Ids are never
reused or renumbered.

## Files

- `01-settled.md` — choices that are not re-litigated (E-01 – E-06).
- `02-open.md` — decisions a phase waits on, with options and a recommendation. `/plan-run` reads it at each stop.
- `03-deferred.md` — work parked behind a trigger (E-20 onward).

## Open decisions → blocked phases

This table is authoritative; `/plan-run` reads it before unlocking a phase.

| Decision | Blocks | Status |
|---|---|---|
| E-07 Picking mechanism | P01 | decided: CPU pick (2026-10-12) |
| E-08 Scene file format | P03 | decided: zip container (2026-10-13) |
| E-09 Mesh import formats and readers | P04 | decided: OBJ, PLY, STL; 3MF deferred (2026-10-13) |
| E-10 Smooth booleans on non-metric inputs | P05 | decided: wrap in `normalize` automatically (2026-10-13) |
