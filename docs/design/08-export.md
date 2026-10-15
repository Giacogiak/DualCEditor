# Export

How the scene becomes a mesh file (P06 T1, `../roadmap/05-plan-record/08-p06-export.md`). The editor
chooses what to contour and where; DualC samples, contours and writes (E-05). Usage:
`../command_reference/01-viewport.md` § Export.

## What is contoured

`exportGraph(scene, ids)` (`src/mesh_export.cpp`) builds one DualC graph in world mm:

- **One object** is posed as `operationGraph` poses an operand: `transform` over `scale`,
  each only when it is not the identity. The code builds the one-input union and takes the
  input out of it, so pointers and edit keys follow the operations' rules
  (`04-scene-model.md`).
- **The visible scene** is the union of the visible objects, in scene order, folded left.
  It is sharp, as the viewport's `min` is.
- **Parameter edits** are uniforms beside the graph (`04-scene-model.md`). For export they
  are written into the params they drive: a number param takes a one-float edit, a vector
  param an edit of its size. An edit that names no param of matching shape is counted and
  logged, not written. At the pinned DualC every binding's `paramKey` is a param, so the
  count is 0.

`buildExportField` builds the graph through `FieldGraph::build` with a
`GraphDirMeshResolver` (library meshes by id, files beside the first object). It is built
once, on the GUI thread, so a graph error is reported before the dialog opens. The resolver
lives beside the field in `ExportField`, because DualC's mesh sources hold bare
references. The default bounds are the field's own bounds padded by 5%, the trace box
`traceBounds` gives the viewport.

## The writers

`runExport` calls one of DualC's host-side writers (`example_common.h`):

- **`writeField`** for a whole export, in the format the extension names (`.stl`, `.3mf`,
  `.obj`). It fills `dualc::Diagnostics`: triangle count, open and non-manifold output
  edges, `emptyContour`. The window logs them.
- **`writeFieldTiledStl` / `writeFieldTiled3mf`** when `Tiled` is on (E-27). Peak RAM is
  one tile of `tileDepth` levels; DualC derives `tilesPerAxis` = ⌈2^depth / (2^tileDepth −
  2)⌉. These writers report no Diagnostics. OBJ has no tiled writer, so it is written
  whole.

Every writer writes `<path>.part` and renames it over `<path>` only on return code 0. A
cancel (rc 3), a writer failure (rc 2) or an unbounded field (rc 1) therefore leaves
nothing new at `<path>`, and a file already there is untouched. DualC's `lastError()`
(thread-local) gives the message of a failure.

Sampler: `rootBounds` = the dialog's bounds, `maxDepth` = its depth, the rest DualC's
defaults. Contourer: DualC's defaults (manifold DC on, no simplification).

## The estimate

`probeExport` contours the field once, at the probe depth min(depth − 1, 5), and keeps a
centroid for each triangle the writer would emit (n − 2 per n-gon). `estimateExport` grows
the count by 4 per level up to the depth: a surface in a fixed box has four times the faces
when the cells halve. RAM is `kExportBytesPerFace` (3072 B, DualC's `kBytesPerSurfaceFace`,
its transient contour peak per output face with a 1.6× margin) times the faces one contour
holds:

- the whole mesh, for a whole export;
- the busiest tile, for a tiled one: the probe's centroids bucketed into DualC's tile grid,
  as DualC's `--mem` picker does.

Measured on the `export` ctest's case (a 20 mm box filled with DualC's default gyroid for it,
wavelength ≈ 3.46 mm): probe 64 140 triangles at depth 5, estimate 256 560 at depth 6,
real 288 732, ratio 0.89. The growth is under 4 when the probe under-resolves the walls,
and over 4 (up to DualC's 8 for a space-filling lattice) when the walls get finer than a
cell at the target depth. The 2× bound is checked on this case only.

The dialog caches probes by probe depth for the current bounds. A bounds edit re-probes
300 ms after the last keystroke. The probe runs on the GUI thread; depth 5 keeps it short
on the test case, but a heavy mesh graph would stall the dialog for as long as a depth-5
contour takes.

## The job

`ExportJob` (`src/export_job.cpp`) runs `runExport` on a `QThread` and is the
`dualc::ProgressSink` itself. DualC calls `report` on the worker; the job queues it to the
thread that owns it, as `progress(permille, stage)`:

- Sample is 0–45%, Contour 45–95%, Write 95–100%;
- a tiled export is its tile count.

`cancel()` requests the `dualc::CancelToken`. Destroying a running job cancels it and waits
for the thread; `MainWindow`'s destructor does this. The window shows a window-modal
`QProgressDialog` (shown, not exec'd, as Make Watertight's is), so the event loop and the
viewport keep running. One export runs at a time.

## Tests

`tests/export_test.cpp` (ctest `export`, GL-free, offscreen platform) checks:

- the format;
- the posed graph, the baked edit and the union;
- the depth-6 fill written to `.stl`, `.3mf` and `.obj`, watertight by DualC's
  diagnostics, and the STL read back and welded closed;
- the estimate within 2×;
- the tiled STL watertight, with the whole export's triangle count;
- a cancel before the start, from the sink in the middle, and over an existing file, whole
  and tiled;
- the job's thread under a turning event loop, and its cancel;
- the dialog's spec, its tiled box per format, and its estimate.
