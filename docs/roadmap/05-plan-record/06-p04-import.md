# P04 — Drag-and-drop mesh import, watertight solids

**DONE (2026-10-13).** Waited on E-09 (mesh formats and readers; decided 2026-10-13 by
delegation: OBJ, PLY and STL, 3MF deferred). A mesh dropped on the window becomes a solid
object that can take part in operations. Design: `../../design/07-mesh-import.md`,
`../../design/05-dcscene-format.md` (imported meshes in the archive). Usage:
`../../command_reference/01-viewport.md` § Importing a mesh, Make Watertight, `--meshes`.
GTX check owed: `../04-gtx-batch-2026-10-13.md` § 5 (E-22's trigger). Development-box
conditions: `README.md`.

## T1. Mesh readers

*Planned:* read E-09's formats into vertex and index buffers, weld coincident vertices,
register each mesh with DualC's `InMemoryMeshResolver` by id so the graph never holds a
path. Acceptance: a ctest loads one small fixture per format and checks the vertex and
face counts after welding; a malformed file gives an error message, not a crash.

**DONE (2026-10-13, commit cd892b3).** Delivered: `src/mesh_import`: `readMeshFile(path)`
(format by suffix) and `readMeshBytes(bytes, format, name)` → `MeshRead` (`MeshBuffers` of
floats and `uint32` indices, the shape `registerMesh` takes, or an `error`, plus what the
weld did). OBJ and STL (binary and ASCII) are our own readers; PLY goes through
geometry-central's happly. The pinned geometry-central would read OBJ and STL too, but it
reads a word in an OBJ vertex as 0 and trusts a binary STL's triangle count (allocating for
any count and reading past the end), so both got their own reader. A PLY header is checked
before happly sees it (`checkPlyHeader`: the declared elements must fit in the file, so a
count of billions is refused, not allocated). Numbers are read in the C locale
(`QByteArrayView::toDouble`), not `strtod`, which would follow the owner's Belgian locale.
The weld (`weldVertices`): polygons fanned into triangles; vertices with identical float
coordinates (`-0` = `0`) merged in order of first use, unused ones dropped; a triangle left
with a repeated vertex dropped and counted; no tolerance, the predictable choice for STL
(the plan says "weld coincident vertices" and does not settle it). `src/mesh_library`:
`addLibraryMesh` returns `mesh-<16 hex of SHA-256>` so the same buffers give the same id;
`findLibraryMesh`; a process-wide map behind a mutex (the compile thread resolves ids),
never forgetting a mesh so an undone import can be redone.
`GraphDirMeshResolver::resolveId` finds the id in the library and registers it with an
owned `InMemoryMeshResolver` on first use, so lowering and picking take ids with no other
change. The codegen's `path` adapter (`meshIdsAsPaths`, `kMeshIdPath`): DualC's GLSL
codegen at the pin refuses `id` on `mesh` and `winding` (`emitMesh` / `emitWinding` in
`examples/field_glsl.cpp`: "'mesh' requires a 'path'") although `FieldGraph::build` takes
it, so `lowerScene` gives the codegen a copy of the graph with each id written as the path
`dualceditor-mesh-id:<id>`, which `resolve` sends back to `resolveId`; the scene's graph
keeps the id, no file is written, DualC is not edited (`../06-dualc-questions.md`). Test
`mesh_import` (GL-free) with fixtures in `tests/data/meshes/` (`cube.obj` with quads written
as `v`, `v//n`, `v/t`, `v/t/n` and negative indices; `cube_binary.ply`; `tetra_ascii.ply`;
`cube_ascii.stl`; `cube_binary.stl` whose header starts with `solid`), written once by a
Python one-off not kept. 17/17 ctests.
Verified: OBJ, binary PLY and ASCII STL read 24 / 24 / 36 vertices and weld to 8 vertices
and 12 triangles; ASCII PLY 4 and 4; binary STL 36 → 8 and 12; all three formats give the
same ±10 mm cube; a soup of 9 vertices welds to 4, 3 triangles to 2, 1 collapses. 24
malformed inputs each give their expected message (OBJ: empty, an index out of range, a
word for a number or index, a two-vertex face, a relative index before the first vertex,
1e39, every triangle collapsing, binary junk; PLY: empty, no `end_header`, 4 000 000 000
vertices declared, an index out of range, a negative index, no face element, cut short;
STL: empty, truncated binary, a count of 2³²−1, no triangles, a facet with 2 vertices, a
word for a number, cut short, no facets; a missing file; a `.3mf`). `mesh(id=…)` builds the
cube's distance (−10 at the centre, +10 10 mm outside); an unknown id is a `GraphError`; a
scene of `mesh(id=…)` and `winding(id=…)` lowers with no `path` in the graph or the shader.
No performance numbers: no large mesh was read or timed (the file and the soup are held in
memory, the weld is one hash-map pass).
- *Wrong on the way:* the codegen was assumed to take `id` because `field_graph.h`
  documents `id=` sources and `InMemoryMeshResolver`; the first test run aborted in
  `lowerScene` (`emitMesh` reads only `path`); the adapter is the fix. A test opened a
  fixture with `QFile::open` unchecked (a `nodiscard` warning). The message for a bad
  relative OBJ index read "vertex index 0 is out of range", an index the file does not
  hold; it reads "relative index -4 reaches before the first vertex", and a negative PLY
  index "a negative vertex index".
- *Open:* the weld has no tolerance, so a mesh whose shared corners differ in the last bit
  stays unwelded, is not watertight, and imports as a `winding` object. 3MF stays deferred
  (E-09). The `.dcscene` did not carry library meshes until T2.

## T2. Import as a solid

*Planned:* on drop, read DualC's `Diagnostics` (watertight and non-manifold counts); a
watertight, oriented mesh becomes a `mesh` node, any other a `winding` node; offer "Make
watertight" on a `winding` object (contour the winding field, clean the mesh, replace the
object with a `mesh` node) on a worker thread with DualC's `ProgressSink` and
`CancelToken`; bakes cannot be cancelled (DualC D-45), say so in the UI. Acceptance:
dropping a closed mesh and a soup gives one `mesh` and one `winding` object and the panel
shows which; Make watertight runs without freezing the UI, reports progress and can be
cancelled during contouring; the preview badge shows the grid resolution; the owner times
1, 3 and 5 imported mesh objects on the GTX at 0.35 and 1.0 through a `--meshes` family,
rows in `../02-measurements.md`, and says whether E-22's trigger fired (`../03-go-no-go.md`
item 3).

**DONE (2026-10-13, commit 585309d), GTX check owed.** Delivered: `src/mesh_solid`
(GL-free): `checkMesh` reads DualC's open and non-manifold edge counts from a `MeshSource`
(`boundaryEdgeCount`, `nonManifoldEdgeCount`) into a `dualc::Diagnostics`
(`Diagnostics` itself is only filled by a sample, far dearer than building the `MeshSource`
the sample would build); DualC reports no orientation, so ours: misoriented edges (two
triangles walking an edge the same way) and the signed volume, because watertight alone
would let an inside-out cube through as a `mesh`; `importMeshFile` / `importMeshBuffers`
(read, check, add to the library, build `mesh(id=…)` or `winding(id=…)`); `meshObjectKind`;
`makeWatertight` (DualC's `WindingNumberField` over the mesh, `dualContourField` at depth
7 with the GWN sign, polygons fanned, `weldVertices` as the clean, cancel token and
progress sink passed through). `MainWindow`: drops, `addFile` and arguments accept mesh
suffixes and call `importMesh`, which logs the read and weld counts and the check; a first
argument that is a mesh is added, not opened; `makeWatertight(id)` runs a `WatertightJob`
on a `QThread::create` thread with a window-modal `QProgressDialog` shown, not `exec`'d
(Sample the first half of the bar, Contour the second; its text says the preview bake that
follows cannot be cancelled, D-45); progress posted by a queued call; the destructor cancels
and joins; the action enabled on a selected `winding` object with no job running; the
timing label ends `mesh grid N³`. `SceneDocument::replaceGraph` takes a verb (`Make
watertight <name>`). `ObjectsPanel`: a delegate draws `[mesh]` / `[winding]` after the name
(the editor gets the name alone; `kind(id)` for tests; the tooltip explains the kind).
`scene_file`: an `id` source's buffers are written as `meshes/<id>.obj` (9 significant
digits, exact float round trip) and read back into the library under the same id.
`--bench` and `--snapshot` take mesh files as objects. `scripts/measure_viewport.py --meshes`:
1, 3 and 5 closed UV spheres (radius 0.4, 24 × 48, OBJ) at P01's primitive positions, at
0.35 and 1.0, unit `P04.2`, with the growth line. Tests: `mesh_solid` (a closed cube
becomes a `mesh`, volume 8000 mm³; an open box with 4 open edges, an inside-out cube and
one flipped triangle with 3 misoriented edges become `winding`; an empty mesh is no solid;
Make Watertight closes the open box and reports both stages to (total, total); a token
requested at the first Contour report throws `Cancelled`; a `.dcscene` round trip keeps
kinds and ids); `main_window` additions (a closed and an open OBJ give `mesh` and
`winding` with matching dock kinds; `mesh grid 96³`; a 1 ms `QTimer` keeps firing while
Make Watertight runs and the dialog shows progress; a second job is refused; the result is
a `mesh` object in one undo step and undo brings the winding back; clicking Cancel leaves
object and history alone and the status says "cancelled"). 18/18 ctests.
Verified against the criteria: the window test's kinds and dock; 348–370 ticks of the 1 ms
timer while Make Watertight ran (open box, depth 7, two runs), the dialog at 1000/1000,
Cancel leaving the object `winding` with no undo step; cancellation inside Contour
specifically in `mesh_solid` (the window test cancels at once, so it stops in Sample: a
deterministic window-level cancel during Contour would race the job); the D-45 sentence in
the dialog; the timing label `compile 94.7 ms   frame 31.8 ms   mesh grid 96³` (the
permanent timing label is the badge). Development box: `mesh_solid_test` 0.32 s including
Make Watertight of the 10-triangle open box at depth 6 (11 096 vertices, 22 188 triangles,
watertight and oriented); in the window at depth 7 on the same box 90 828 triangles in
about 1 s; the preview grid of every mesh node is 96³ (`kDefaultMeshGridRes`). The
`--meshes` family was smoke-run (3 spheres, 2 frames, 320×200: imports and draws).
- *Wrong on the way:* the test called `QProgressDialog::cancel()`, a slot that resets the
  dialog without emitting `canceled`, so the token was never requested; it clicks the
  Cancel button as a person does. `importMeshFile` moved out of `read.mesh` and then moved
  `read`; GCC 15 warned `maybe-uninitialized` through the optional; it copies the buffers.
- *Open:* **E-22 cannot be judged until the GTX check** (§ 5 of the batch). A contour that
  comes out non-manifold stays a `winding` node over the new mesh (closed, but `MeshSource`
  would reject it); not seen. Make Watertight's depth (7) and the 96³ preview grid are
  fixed in the UI and untimed on a large mesh. `checkMesh` builds a `MeshSource` (a BVH) on
  the GUI thread at drop time, cheap for the fixtures, untimed for millions of triangles.
  The DualC question from T1 (`emitMesh` / `emitWinding` resolving `id=`) stands.
- **Window check owed, any machine** (`../04-gtx-batch-2026-10-13.md` § 10): the dock
  reads `name  [mesh]` and `name  [winding]`; `Edit ▸ Make Watertight` on the winding one
  shows "Sampling…", then "Contouring…", and the D-45 sentence, with the viewport live
  behind it; Cancel stops it and a second run replaces the object with a `[mesh]` one; the
  status bar ends `mesh grid 96³`.

## Exit criteria

T1 and T2 met as above, apart from T2's owed GTX timing; `python scripts/check.py` green
(18/18 ctests). An imported object is `mesh(id=…)` or `winding(id=…)`, lowered as
`transform(scale(graph))` like any object, so P05's operations take it as an operand.

---
← Back to the [Plan record](README.md) · [Roadmap index](../README.md).
