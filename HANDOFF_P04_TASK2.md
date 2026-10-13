# Handoff — P04 T2: import as a solid

Date: 2026-10-13. Plan: `plan/50_P04_import.md` § T2. DualC pin: `2fcb6ee` (unchanged).

**Status: done, GTX check owed.** A dropped OBJ, PLY or STL becomes a `mesh` object when
DualC finds it watertight and it is oriented outward, and a `winding` object otherwise.
The Objects dock shows `[mesh]` or `[winding]` after the name. `Edit ▸ Make Watertight`
contours a winding object on a worker thread, with a progress dialog whose Cancel stops
the sampling or the contouring, and replaces it with a `mesh` object in one undo step.
The status bar names the preview grid (`mesh grid 96³`). `.dcscene` now carries imported
meshes (T1's open item). `python scripts/check.py` exits 0 on the dev box (NV92), 18/18
ctests. P04's exit criteria hold except the timing, which is the human check below.

## What changed and why

- **`src/mesh_solid.{h,cpp}` (new, GL-free):**
  - `checkMesh`: DualC's open and non-manifold edge counts, read from a `MeshSource`
    (`boundaryEdgeCount`, `nonManifoldEdgeCount`), held in a `dualc::Diagnostics`
    (`inputBoundaryEdges`, `inputNonManifoldEdges`, `inputWatertight`, `inputEmpty`).
    `Diagnostics` itself is only filled by a sample (`sampleMeshToHermiteOctree`), far
    dearer than building the `MeshSource` the sample would build. DualC reports no
    orientation, so ours: misoriented edges (two triangles walking an edge the same way)
    and the signed volume. The plan asks for "watertight, oriented"; watertight alone
    would let an inside-out cube through as a `mesh`.
  - `importMeshFile` / `importMeshBuffers`: read, check, add to the library, and build
    `mesh(id=…)` or `winding(id=…)`.
  - `meshObjectKind`: the kind the dock shows.
  - `makeWatertight`: DualC's `WindingNumberField` over the mesh, `dualContourField`
    (depth 7, GWN sign), polygons fanned, `weldVertices` as the "clean". Cancel token and
    progress sink are passed through to DualC.
- **`MainWindow`:** drops (and `addFile`, and command-line arguments) accept mesh
  suffixes and call `importMesh`, which logs the read and weld counts and the check.
  `makeWatertight(id)` runs a `WatertightJob` on a `QThread::create` thread. A
  window-modal `QProgressDialog` is shown, not `exec`'d, so the event loop keeps running.
  Sample is the first half of the bar and Contour the second. Its text says the preview
  bake that follows cannot be cancelled (DualC D-45). Progress is posted to the GUI thread
  by a queued call. The destructor cancels and joins. `showTiming` appends `mesh grid N³`.
  The action is enabled on a selected `winding` object with no job running.
- **`SceneDocument::replaceGraph`** takes a verb, so the step reads
  `Make watertight <name>`, not `Reload <name>`.
- **`ObjectsPanel`:** a delegate draws `[kind]` after the name. The editor gets the name
  alone, so renaming is unchanged. `kind(id)` is for tests. The tooltip explains the kind.
- **`scene_file`:** an `id` source's buffers are written as `meshes/<id>.obj` (9
  significant digits, exact float round trip), and read back into the library on load
  under the same id. Without this, a saved scene with an import loaded only in the
  process that made it.
- **`main.cpp`:** `--bench` and `--snapshot` take mesh files as objects, so
  `measure_viewport.py --meshes` works. In the window, a first argument that is a mesh is
  added, not opened.
- **`scripts/measure_viewport.py --meshes`:** scenes of 1, 3 and 5 closed UV spheres
  (radius 0.4, 24 × 48, OBJ) at P01's primitive positions, at 0.35 and 1.0, unit `P04.2`,
  with the growth line.
- **Tests:**
  - **New `mesh_solid` ctest:** a closed cube becomes a `mesh` (volume 8000 mm³); an open
    box (4 open edges), an inside-out cube and one flipped triangle (3 misoriented edges)
    become `winding`; an empty mesh is no solid. Make Watertight closes the open box and
    reports both stages to (total, total). A token requested at the first Contour report
    throws `Cancelled`. A `.dcscene` round trip keeps kinds and ids.
  - **`main_window` additions:** dropping a closed and an open OBJ gives `mesh` and
    `winding`, and the dock kinds match. The status bar shows `mesh grid 96³`. While Make
    Watertight runs, a 1 ms `QTimer` keeps firing and the dialog shows progress. A second
    job is refused. The result is a `mesh` object in one undo step, and undo brings the
    winding back. Clicking Cancel leaves the object and the history alone, and the status
    says "cancelled".
- **Docs:** `docs/design/07-mesh-import.md` (§ Import as a solid, Make watertight, Preview
  grid, Saving imported meshes), `05-dcscene-format.md` (the `meshes/<id>.obj` entry),
  `docs/command_reference/01-viewport.md` (drops, *Importing a mesh*, Make Watertight,
  `mesh grid`, undo row, `--meshes`), `docs/roadmap/02-measurements.md` (the owed P04 T2
  table), and the roadmap focus.

## Acceptance criteria and exit criteria

| Criterion | Evidence |
|---|---|
| Dropping a closed mesh and a soup gives one `mesh` and one `winding` object; the object panel shows which | `main_window`: "the closed mesh is a mesh object, the open one a winding object" and "the Objects dock shows which is which" (`[mesh]` / `[winding]`). `mesh_solid` covers the classification cases. |
| "Make watertight" runs without freezing the UI, reports progress, can be cancelled during contouring | `main_window`: 348–370 ticks of a 1 ms timer while it ran (dev box, open box, depth 7, two runs), the dialog reached 1000/1000, and the Cancel button left the object `winding` with no undo step. Cancellation inside the Contour stage specifically: `mesh_solid` requests the token at the first Contour report and gets `dualc::Cancelled`. The window test cancels at once, so it stops in Sample: a deterministic window-level cancel during Contour would race the job. |
| Bakes cannot be cancelled (DualC D-45); say so in the UI | The dialog's text: "The preview bake that follows cannot be cancelled (DualC D-45)." Seen in code; a person sees it in the dialog. |
| The preview badge shows the grid resolution | `main_window`: timing label `compile 94.7 ms   frame 31.8 ms   mesh grid 96³`. There was no badge before; the status bar's permanent timing label is that badge. |
| Human check: 1, 3, 5 imported meshes at 0.35 and 1.0 on the GTX; rows in `02-measurements.md`; whether E-22 fired | **Owed** (§ GTX check owed). The `--meshes` family exists and was smoke-run on the dev box (3 spheres, 2 frames, 320 × 200: it imports and draws; not a measurement, E-04). |
| Exit: T1 and T2 met | T1: `HANDOFF_P04_TASK1.md`, its ctest still passes. T2: above, apart from the owed GTX timing. |
| Exit: `python scripts/check.py` is green | Exit 0, 18/18 ctests, dev box. |

The plan's criteria were right. The plan file is unchanged.

## Measured (dev box, NV92 / Quadro FX 3700, Mesa 26.0.8; no performance meaning, E-04)

- `mesh_solid_test` in full: 0.32 s wall. It includes Make Watertight of the 10-triangle
  open box at depth 6: 11 096 vertices, 22 188 triangles, watertight and oriented.
- Make Watertight in the window at depth 7 on the same box: 90 828 triangles, about 1 s.
- Preview grid of every mesh node: 96³ (`kDefaultMeshGridRes`).

## What I got wrong on the way

- **The test called `QProgressDialog::cancel()`.** That slot resets the dialog; it does
  not emit `canceled`, which is the signal the button sends. So the token was never
  requested and the job finished. The test now clicks the dialog's Cancel button, as a
  person does.
- `importMeshFile` first moved out of `read.mesh` and then moved `read`. GCC 15 warned
  `maybe-uninitialized` through the optional. It now copies the buffers into the library
  and keeps `read` whole.

## Open

- **E-22 cannot be judged yet:** that is the GTX check.
- A contour that comes out non-manifold stays a `winding` node over the new mesh. It is
  closed, but `MeshSource` would reject it. This has not been seen.
- Make Watertight's depth (7) is fixed in the UI, and so is the 96³ preview grid. Neither
  is timed on a large mesh.
- The DualC question from T1 stands: should `emitMesh` and `emitWinding` resolve `id=`?
- `checkMesh` builds a `MeshSource` (a BVH) on the GUI thread at drop time. That is cheap
  for these fixtures. For a mesh of millions of triangles it could stall the drop, and it
  has not been timed.

## For the next unit

- P04 is complete. An imported object is `mesh(id=…)` or `winding(id=…)`, and
  `meshObjectKind(graph)` tells which. Operations wrap it like any graph
  (`operationGraph`), so the P05 fills take it as an operand.
- `importMeshBuffers(MeshBuffers)` imports buffers that do not come from a file.
- go-no-go item 4 (P05 T2) times the octet fill of an imported mesh. The `--meshes`
  spheres are a ready input.

## Human check

A person should confirm in the window, on any machine:
1. Drop a closed mesh and an open one: the dock reads `name  [mesh]` and `name  [winding]`.
2. Select the winding one and run `Edit ▸ Make Watertight`. The dialog shows "Sampling…",
   then "Contouring…", and the D-45 sentence. The viewport stays live behind it. Cancel
   stops it, and a second run replaces the object with a `[mesh]` one.
3. The status bar's right side ends `mesh grid 96³`.

## GTX check owed

On the GTX 1050 Ti laptop, at this commit, in a Release build:

```bash
python scripts/measure_viewport.py --meshes --prime
```

1. Paste the 6 rows (1, 3, 5 imported meshes × 0.35, 1.0) and the growth lines into
   `docs/roadmap/02-measurements.md` § Imported meshes (P04 T2).
2. Compare the settled GPU ms at 1.0 against the frame target in
   `docs/roadmap/03-go-no-go.md`. For reference, P00.5 measured one mesh object settling
   at 7.3 GPU ms. If 3 or 5 meshes miss the target, E-22 (freeze an object to a baked
   grid) has fired: record that in `03-go-no-go.md` item 3 and in
   `docs/decisions/03-deferred.md`.
3. Expect the culling of P07 to keep the growth per added mesh object well below the
   cost of a single one. Each mesh node is costly, so it is culled outside its box.
