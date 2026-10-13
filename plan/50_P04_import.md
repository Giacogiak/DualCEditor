# P04 — Drag-and-drop mesh import, watertight solids

Waits on **E-09** (mesh formats and readers). A mesh dropped on the window becomes a solid
object that can take part in operations.

### T1. Mesh readers

Read the formats chosen in E-09 into vertex and index buffers. Weld coincident vertices.
Register each mesh with DualC's `InMemoryMeshResolver` by id, so the graph never holds a
file path.

#### Acceptance criteria
- A ctest loads one small fixture per format, and checks the vertex and face counts after
  welding.
- A malformed file gives an error message, not a crash.

### T2. Import as a solid

On drop, read DualC's `Diagnostics`: watertight and non-manifold counts. Then:
- **A watertight, oriented mesh** becomes a `mesh` node.
- **Any other mesh** becomes a `winding` node.

Offer "Make watertight" on a `winding` object. It contours the winding field, cleans the
mesh, and replaces the object with a `mesh` node on that result. It runs on a worker
thread, with DualC's `ProgressSink` and `CancelToken`. Bakes cannot be cancelled (DualC
D-45); say so in the UI.

#### Acceptance criteria
- Dropping a closed mesh and a soup gives one `mesh` object and one `winding` object. The
  object panel shows which is which.
- "Make watertight" runs without freezing the UI, reports progress, and can be cancelled
  during contouring.
- The preview badge shows the grid resolution the mesh is previewed at.
- Human check: the owner times scenes of 1, 3 and 5 imported mesh objects on the GTX
  1050 Ti at preview scales 0.35 and 1.0, through a `--meshes` family in
  `scripts/measure_viewport.py`. The rows go in `docs/roadmap/02-measurements.md`, and the
  handoff says whether E-22's trigger fired (`docs/roadmap/03-go-no-go.md`, item 3).

## Exit criteria
- Tasks T1 and T2 are met. `python scripts/check.py` is green.
