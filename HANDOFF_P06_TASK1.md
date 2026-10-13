# Handoff — P06 T1: export dialog

Date: 2026-10-13. Plan: `plan/70_P06_export.md` § T1. DualC pin: `2fcb6ee` (unchanged).
Code commit: `9c40704`.

**Status: done. P06's exit criteria hold, and P06 was the last phase of the plan.**
`File ▸ Export Selected Object…` and `Export Visible Scene…` contour through DualC's
writers on a worker thread, with progress and a Cancel that leaves no file.
`python scripts/check.py` exits 0 on the dev box (NV92), with 20/20 ctests.

## What changed and why

- **`src/mesh_export.{h,cpp}` (new, GL-free):** the export core.
  - `exportGraph(scene, ids)`:
    - one object is posed as `operationGraph` poses an operand (`transform` over `scale`);
    - several objects are their union;
    - parameter edits are written into the params they drive. They are uniforms beside
      the graph, and DualC's writers read the graph only. Without this, the file would
      not be what the viewport shows.
  - `buildExportField`: `FieldGraph::build` with a `GraphDirMeshResolver` that lives with
    the field. Default bounds are the field's bounds padded by 5%, as `traceBounds`.
  - `exportFormat` (by extension) and `exportTileable` (STL, 3MF).
  - `probeExport` and `estimateExport`, the estimate:
    - one coarse contour at min(depth − 1, 5);
    - grown ×4 per level;
    - RAM at DualC's 3 KiB per face;
    - for a tiled export, the busiest tile, bucketed as DualC's `--mem` picker does.
  - `runExport`: `writeField`, or `writeFieldTiledStl` / `writeFieldTiled3mf`, mapped to
    `Written` / `Cancelled` / `Failed` with DualC's `lastError()`.
- **`src/export_job.{h,cpp}` (new):** `ExportJob`, a `QObject` that runs `runExport` on a
  `QThread`.
  - It is DualC's `ProgressSink`, and queues `progress(permille, stage)` to its own thread.
  - `cancel()` requests the token.
  - Destroying a running job cancels it and waits.
  - It is GL-free so the ctest can drive it under an event loop.
- **`src/export_dialog.{h,cpp}` (new):** `ExportDialog`.
  - **File:** `Browse…`, and the `Format` the extension names.
  - **Depth:** 3–12, default 7, with the live estimate (`about N triangles, M RAM, cell C mm`).
  - **Bounds:** six mm spin boxes and `Reset`.
  - **Tiled:** `Tiled, one tile in RAM` and its tile depth, greyed out for OBJ.
  - `Export` is greyed out for an unknown extension or an empty box. Probes are cached per
    bounds; a bounds edit re-probes after 300 ms.
- **`MainWindow`:** `File ▸ Export Selected Object…` (`Ctrl+E`) and `Export Visible
  Scene…` (`Ctrl+Shift+E`).
  - `exportDialog(visible)` and `exportObjects(ids, spec)`.
  - A window-modal `QProgressDialog`, shown and not exec'd, as Make Watertight's.
  - Log and status lines with DualC's diagnostics.
  - The `exportFinished(bool)` signal.
  - The destructor stops a running export.
- **Tests:** `tests/export_test.cpp`, ctest `export` (GL-free, offscreen platform),
  25 checks, 32.6 s in the gate.
- **Docs:**
  - `docs/design/08-export.md` (new) and its index line;
  - `docs/command_reference/01-viewport.md`: § Export, and the `File` menu row;
  - E-27 closed in `docs/decisions/03-deferred.md`;
  - the roadmap focus records the end of the plan.

## Acceptance criteria (T1)

| Criterion | Evidence |
|---|---|
| A TPMS-filled mesh at depth 6 exports watertight; a ctest checks it through DualC's diagnostics on a small case | `export`, on a 20 mm box moved to x = 30 and filled with DualC's default gyroid for it (`fillGraph(defaultFill("gyroid", box))`, wavelength ≈ 3.46 mm, t ≈ 0.69 mm). At depth 6, `.stl`, `.3mf` and `.obj` each come back with rc 0, 288 732 triangles, `outputWatertight` (0 open, 0 non-manifold edges) and no `.part`. The STL, read back by the editor's reader and welded, is `watertight` with the same count. |
| Cancelling leaves no partial file, through DualC's `.part` rename | `export`, for both whole and tiled STL: a cancel before the start (rc 3, "cancelled (sampling)"), and one requested from the sink at the first Contour or Tile report (in the middle), leave no file and no `.part`. A cancelled export over an existing file leaves its bytes as they were. An `ExportJob` cancelled at its first progress report leaves no file. |
| The depth estimate is within 2× of the real face count on the test case | `export`: a depth-5 probe gives 64 140 triangles, the estimate for depth 6 is 256 560, and DualC wrote 288 732. Ratio **0.889**. |
| Export runs on a worker thread with progress and cancel, and the UI stays responsive | `export` § 7: a depth-6 `ExportJob` writes its file while a 5 ms `QTimer` on the starting thread keeps firing; its progress reports arrive in order and end at 1000. The window side, with the GTX timing, is the human check below. |
| Tiled STL for large lattices closes E-27 | `export`: tiled STL at depth 6, tile depth 4 (5³ = 125 tiles) has the whole export's 288 732 triangles and is watertight once read back and welded. The estimate charges the busiest tile (3 420 of 256 560 faces). E-27 is closed in `03-deferred.md`. |

## Exit criteria (P06), one bullet at a time

- **"T1 is met. `python scripts/check.py` is green."** T1: the table above. Gate:
  `[gate] all checks passed`, 20/20 ctests, 227.7 s ctest total (229.7 s wall for the
  check) on the dev box.
- **"`docs/roadmap/README.md` records the end of the plan."** Its *Current focus* opens
  with "The plan is done (2026-10-13)" and lists what is still open: queued GTX checks,
  E-28, the cold-compile question and the GTX `main_window` failure. It is followed by the
  P06 entry.

## Measured (dev box, NV92 / Quadro FX 3700, Mesa 26.0.8; CPU-only work, but this box is not the reference, E-04)

All on the `export` test case: the gyroid fill above, bounds = its world box padded by 5%,
DualC's default sampler and contourer, all hardware threads.

| What | Result |
|---|---|
| depth 6 `.stl` | 2 594 ms, 288 732 triangles |
| depth 6 `.3mf` | 4 023 ms |
| depth 6 `.obj` | 6 497 ms |
| depth 7 STL through `ExportJob` (an earlier run of the test, before I lowered it to 6) | 13 588 ms, 1 150 620 triangles: ×3.99 per level from depth 6 |
| estimate / real at depth 6 | 0.889 |
| `export` ctest | 32.6 s |

## What I got wrong on the way

- **The test first read `MeshImport::read.mesh` as a value.** It is a
  `std::optional<MeshBuffers>`. The compile error caught it.
- **The test first ran its `ExportJob` at depth 7**, which took 13.6 s and made the ctest
  44 s. Depth 6 checks the same property, and the ctest now takes 33 s.
- **The cell size the dialog shows** is the longest side of the bounds over 2^depth. I did
  not check whether DualC's sampler cubes a non-cubic root box. If it does not, the cell is
  smaller along the short axes. The number is informative only; the estimate does not
  depend on it.

## Open

- **The estimate is checked on one case.** Growth is ×4 per level. A part whose walls or
  struts are finer than a probe cell is under-resolved at depth 5. Its count can then grow
  faster than ×4 between the probe and the target (DualC's own RAM picker assumes up to
  ×8). A lattice fill at depth 9 or more is the case to measure on the GTX (below).
- **The probe runs on the GUI thread.** It is at most depth 5, and quick on the test case.
  A heavy `mesh` or `winding` graph could stall the dialog for one depth-5 contour. Move the
  probe to a thread if the owner sees it.
- **Tiled exports have no Diagnostics.** DualC's tiled writers do not take a
  `Diagnostics*`, so the log says "tiled, tile depth D" instead of a triangle count and
  watertightness. A `diag` parameter on `writeFieldTiledStl` and `writeFieldTiled3mf` would
  fix that. It is a DualC change (rule 5), for the owner.
- **`kExportBytesPerFace` mirrors DualC's `kBytesPerSurfaceFace`.** DualC's constant is
  file-local. `chooseTileDepthForBudget` is public, but it only picks a depth and prints
  its estimate. Exposing the estimate would need a DualC PR.
- **Parameter edits that name no graph param** are counted and logged, not exported. At the
  pinned DualC there are none.
- **No `main_window` check drives the menu entries.** The ctest covers the core, the job and
  the dialog. The window's wiring (the menu, the progress dialog, Cancel, the log) is the
  human check below.

## For the next unit

There is no next unit in the plan: P06 was the last phase. If one is added:
- `exportGraph` + `buildExportField` + `runExport` is the whole path from a scene to a file.
  A command-line `--export` would be those three calls.
- `ExportJob` is the template for any other DualC job on a thread, and Make Watertight's
  `WatertightJob` could move onto it.

## Human check

On any machine, in the window:
1. Drop or open a graph, select it, then `File ▸ Export Selected Object…` (`Ctrl+E`). The
   dialog shows `File` ending in `.stl` beside the scene or in Documents, `Format: Binary
   STL`, `Depth 7`, an estimate line, the bounds, and `Tiled` unchecked. Change the depth:
   the estimate changes about ×4 per step. Rename the file `.obj`: `Tiled` greys out.
   Rename it `.ply`: `Export` greys out.
2. `Export` at depth 7: the progress dialog shows `Sampling`, `Contouring`, `Writing`; the
   viewport still orbits during it. The log gets `export <file>: depth 7 in T ms: N
   triangles, watertight`. Open the file in a slicer or viewer: it is the object as shown,
   in its moved pose and with any Parameters edits.
3. Export at depth 9 and press `Cancel` mid-way: the status bar says no file was written,
   and neither `<file>` nor `<file>.part` exists.
4. Tick `Tiled`, tile depth 5, depth 8, `.3mf`: the progress shows `Tile i of n`, and the
   file opens in a slicer as one part.
5. `Export Visible Scene…` (`Ctrl+Shift+E`) with two visible objects: one file holding both.

## GTX check owed

On the GTX 1050 Ti laptop, at this commit, Release build:
1. Open a scene with a 20 mm box, then `Operations ▸ Fill with Lattice…` at its default
   octet. Select the fill, `Ctrl+E`, `.stl`:
   - at depth 8, note the dialog's estimate (triangles, RAM);
   - export, and note the log's time and triangle count.

   **Expected:** the estimate is within 2× of the logged count. If it is not, record both
   numbers. The ×4 growth model is then wrong for lattices, and the estimate needs the
   probe's own growth ratio.
2. The same fill at depth 9, whole and then tiled at tile depth 6. Note the time and the
   peak RAM (`/usr/bin/time -v`, or the system monitor). **Expected:** the tiled peak is well
   under the whole one and under the dialog's tiled estimate. Both files are watertight in
   a slicer.
3. While the depth-9 export runs, orbit the viewport: it keeps drawing, as during a cold
   compile (P07 T2). Cancel it: no file is left.
4. Record the numbers in `docs/roadmap/02-measurements.md` under a P06 heading.

P05's checks, already queued, still stand (`HANDOFF_P05_TASK3.md` § GTX check owed).
