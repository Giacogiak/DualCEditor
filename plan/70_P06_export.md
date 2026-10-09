# P06 — Mesh extraction and export

The editor's output: contour the selected object, or the visible scene, to STL, 3MF or
OBJ using DualC's writers.

### T1. Export dialog

The dialog sets:
- the output format, chosen by extension;
- the octree depth, with an estimate of face count and RAM;
- the bounds;
- tiled STL for large lattices, which closes E-27.

Export runs on a worker thread with progress and cancel, and the UI stays responsive.

#### Acceptance criteria
- Exporting a TPMS-filled mesh at depth 6 writes a watertight file. A ctest checks this
  through DualC's diagnostics on a small case.
- Cancelling leaves no partial file, thanks to DualC's `.part` rename.
- The depth estimate is within 2× of the real face count on the test case.

## Exit criteria
- T1 is met. `python scripts/check.py` is green.
- `docs/roadmap/README.md` records the end of the plan.
