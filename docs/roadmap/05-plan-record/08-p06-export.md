# P06 — Mesh extraction and export

**DONE (2026-10-13).** The editor's output: contour the selected object, or the visible
scene, to STL, 3MF or OBJ through DualC's writers. P06 was the last phase of the plan.
Design: `../../design/08-export.md`. Usage: `../../command_reference/01-viewport.md`
§ Export. GTX check owed: `../04-gtx-batch-2026-10-13.md` § 8. Development-box
conditions: `README.md`.

## T1. Export dialog

*Planned:* the dialog sets the output format (by extension), the octree depth with an
estimate of face count and RAM, the bounds, and tiled STL for large lattices (closing
E-27); export runs on a worker thread with progress and cancel, the UI staying
responsive. Acceptance: a TPMS-filled mesh at depth 6 exports watertight, a ctest checking
it through DualC's diagnostics on a small case; cancelling leaves no partial file (DualC's
`.part` rename); the depth estimate is within 2× of the real face count on the test case.

**DONE (2026-10-13, commits 9c40704 code and c0ff0f4 record), GTX check owed.**
Delivered: `src/mesh_export` (GL-free): `exportGraph(scene, ids)` poses one object as
`operationGraph` poses an operand (`transform` over `scale`), unions several, and writes
parameter edits into the params they drive (they are uniforms beside the graph and DualC's
writers read the graph only; without this the file would not be what the viewport shows);
`buildExportField` (`FieldGraph::build` with a `GraphDirMeshResolver` living with the
field; default bounds the field's padded 5%, as `traceBounds`); `exportFormat` (by
extension) and `exportTileable` (STL, 3MF); `probeExport` and `estimateExport` (one coarse
contour at min(depth − 1, 5), grown ×4 per level, RAM at DualC's 3 KiB per face, for a
tiled export the busiest tile bucketed as DualC's `--mem` picker does); `runExport`
(`writeField`, or `writeFieldTiledStl` / `writeFieldTiled3mf`, mapped to `Written` /
`Cancelled` / `Failed` with DualC's `lastError()`). `src/export_job`: `ExportJob`, a
`QObject` running `runExport` on a `QThread`, DualC's `ProgressSink` queuing
`progress(permille, stage)` to its own thread, `cancel()`, destruction cancels and waits,
GL-free so the ctest drives it under an event loop. `src/export_dialog`: `File` with
`Browse…` and the `Format` the extension names; `Depth` 3–12, default 7, with the live
estimate (`about N triangles, M RAM, cell C mm`); six mm bounds spin boxes and `Reset`;
`Tiled, one tile in RAM` with its tile depth, greyed out for OBJ; `Export` greyed out for an
unknown extension or an empty box; probes cached per bounds, a bounds edit re-probing after
300 ms. `MainWindow`: `File ▸ Export Selected Object…` (`Ctrl+E`) and `Export Visible
Scene…` (`Ctrl+Shift+E`), `exportDialog(visible)`, `exportObjects(ids, spec)`, a
window-modal `QProgressDialog` shown and not exec'd as Make Watertight's, log and status
lines with DualC's diagnostics, `exportFinished(bool)`, the destructor stopping a running
export. Test `export` (GL-free, offscreen platform, 25 checks, 32.6 s). E-27 closed.
20/20 ctests; gate `[gate] all checks passed`, ctest total 227.7 s.
Verified, on a 20 mm box moved to x = 30 and filled with DualC's default gyroid for it
(`fillGraph(defaultFill("gyroid", box))`, wavelength ≈ 3.46 mm, t ≈ 0.69 mm): at depth 6,
`.stl`, `.3mf` and `.obj` each return rc 0, 288 732 triangles, `outputWatertight` (0 open,
0 non-manifold edges), no `.part`; the STL read back by the editor's reader and welded is
watertight with the same count. Cancel, whole and tiled STL: before the start (rc 3,
"cancelled (sampling)") and from the sink at the first Contour or Tile report leave no file
and no `.part`; a cancelled export over an existing file leaves its bytes; an `ExportJob`
cancelled at its first report leaves no file. Estimate: a depth-5 probe gives 64 140
triangles, the estimate for depth 6 is 256 560, DualC wrote 288 732, ratio **0.889**. A
depth-6 `ExportJob` writes while a 5 ms `QTimer` on the starting thread keeps firing, its
reports in order and ending at 1000. Tiled STL at depth 6, tile depth 4 (125 tiles) has the
whole export's 288 732 triangles and is watertight once read back and welded; the estimate
charges the busiest tile (3 420 of 256 560 faces). Development box, same case, DualC's
default sampler and contourer, all hardware threads:

| What | Result |
|---|---|
| depth 6 `.stl` | 2 594 ms, 288 732 triangles |
| depth 6 `.3mf` | 4 023 ms |
| depth 6 `.obj` | 6 497 ms |
| depth 7 STL through `ExportJob` (an earlier run of the test) | 13 588 ms, 1 150 620 triangles: ×3.99 per level from depth 6 |

- *Wrong on the way:* the test first read `MeshImport::read.mesh` as a value (a
  `std::optional<MeshBuffers>`; compile error). The test first ran its `ExportJob` at depth
  7, 13.6 s, a 44 s ctest; depth 6 checks the same property in 33 s. The cell size the
  dialog shows is the longest side of the bounds over 2^depth, without checking whether
  DualC's sampler cubes a non-cubic root box; if it does not, the cell is smaller along the
  short axes; informative only, the estimate does not depend on it.
- *Open:* the estimate is checked on one case; growth is ×4 per level, and a part whose
  walls or struts are finer than a probe cell is under-resolved at depth 5, so its count can
  grow faster than ×4 between probe and target (DualC's own RAM picker assumes up to ×8); a
  lattice fill at depth 9 or more is the case to measure on the GTX (§ 8 of the batch). The
  probe runs on the GUI thread (at most depth 5; a heavy `mesh` or `winding` graph could
  stall the dialog for one depth-5 contour; move it to a thread if the owner sees it). Tiled
  exports have no `Diagnostics` (DualC's tiled writers do not take one), so the log says
  "tiled, tile depth D" instead of a count and watertightness; `kExportBytesPerFace`
  mirrors DualC's file-local `kBytesPerSurfaceFace` (`../06-dualc-questions.md`). Parameter
  edits that name no graph param are counted and logged, not exported (none at the pinned
  DualC). No `main_window` check drives the menu entries: the ctest covers the core, the
  job and the dialog; the window's wiring is the check below.
- **Window check owed, any machine** (`../04-gtx-batch-2026-10-13.md` § 10): with a graph
  selected, `Ctrl+E` shows `File` ending `.stl` beside the scene or in Documents,
  `Format: Binary STL`, `Depth 7`, an estimate line, the bounds, `Tiled` unchecked; changing
  the depth changes the estimate about ×4 per step; renaming to `.obj` greys out `Tiled`,
  to `.ply` greys out `Export`. Export at depth 7: the progress dialog shows `Sampling`,
  `Contouring`, `Writing`, the viewport still orbits, the log gets `export <file>: depth 7
  in T ms: N triangles, watertight`, and a slicer shows the object as shown, in its moved
  pose and with any Parameters edits. Export at depth 9 and `Cancel` mid-way: the status
  bar says no file was written, neither `<file>` nor `<file>.part` exists. `Tiled`, tile
  depth 5, depth 8, `.3mf`: the progress shows `Tile i of n` and a slicer opens one part.
  `Export Visible Scene…` with two visible objects gives one file holding both.
- *If a phase is added:* `exportGraph` + `buildExportField` + `runExport` is the whole path
  from a scene to a file, so a command-line `--export` would be those three calls;
  `ExportJob` is the template for any other DualC job on a thread, and Make Watertight's
  `WatertightJob` could move onto it.

## Exit criteria

T1 met as above; `python scripts/check.py` green (20/20 ctests); `../README.md` records
the end of the plan.

---
← Back to the [Plan record](README.md) · [Roadmap index](../README.md).
