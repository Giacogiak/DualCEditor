# Handoff — P04 T1: mesh readers

Date: 2026-10-13. Plan: `plan/50_P04_import.md` § T1. DualC pin: `2fcb6ee` (unchanged).

**Status: done.** OBJ, PLY and STL files (E-09) read into welded triangle buffers. A
malformed file gives a message that names the file and the fault, not a crash. A read mesh
goes into a library keyed by an id derived from its content. A graph names it as
`mesh(id=…)` or `winding(id=…)`, never as a path, and the id resolves through DualC's
`InMemoryMeshResolver`. The design is in `docs/design/07-mesh-import.md`. Nothing in the UI
changed: the drop is T2's. `python scripts/check.py` exits 0 on the dev box (NV92), with
17/17 ctests.

## What changed and why

- **`src/mesh_import.{h,cpp}`: the readers.**
  - `readMeshFile(path)` picks the format by suffix; `readMeshBytes(bytes, format, name)`
    reads bytes in memory.
  - Both return a `MeshRead`: the `MeshBuffers` (floats and `uint32` indices, the shape
    `InMemoryMeshResolver::registerMesh` takes), or an `error`. It also carries what the
    weld did.
  - Readers: OBJ and STL (binary and ASCII) are our own; PLY goes through geometry-central
    (happly), as E-09 said. The pinned geometry-central would also read OBJ and STL, but it
    reads a word in an OBJ vertex as 0, and it trusts a binary STL's triangle count (it
    allocates for any count, and reads past the end). So OBJ got its own reader as well as
    STL.
  - A PLY header is checked before happly sees it (`checkPlyHeader`): the declared elements
    must fit in the file, so a count of billions is refused, not allocated.
  - Numbers are read in the C locale (`QByteArrayView::toDouble`), not by `strtod`, which
    would follow the owner's Belgian locale.
- **The weld (`weldVertices`):**
  - polygons are fanned into triangles;
  - vertices with identical float coordinates (`-0` = `0`) are merged, in order of first
    use, and unused vertices are dropped;
  - a triangle left with a repeated vertex is dropped and counted.

  There is no tolerance, which is the predictable choice for STL (exact float copies). The
  plan says "weld coincident vertices" and does not settle this.
- **`src/mesh_library.{h,cpp}`: the library.**
  - `addLibraryMesh` returns `mesh-<16 hex of SHA-256>`, so the same buffers give the same
    id.
  - `findLibraryMesh` looks an id up.
  - It is a process-wide map behind a mutex, because the compile thread resolves ids. It
    never forgets a mesh, so an undone import can be redone.
- **`GraphDirMeshResolver::resolveId`** (`src/graph_file`): it finds the id in the library
  and registers it with an owned `InMemoryMeshResolver` on first use. Lowering and picking
  already go through this resolver, so they take ids with no other change.
- **The codegen's `path` adapter** (`meshIdsAsPaths`, `kMeshIdPath`). DualC's GLSL codegen
  at the pin refuses `id` on `mesh` and `winding` (`emitMesh` and `emitWinding` in
  `examples/field_glsl.cpp`: "'mesh' requires a 'path'"); `FieldGraph::build` takes it.
  So `lowerScene` gives the codegen a copy of the graph with each id written as the path
  `dualceditor-mesh-id:<id>`, and `GraphDirMeshResolver::resolve` sends that path back to
  `resolveId`. The scene's graph keeps the id; no file is written. DualC is not edited
  (rule 5). See § Open for the question to the DualC owner.
- **Tests:** new GL-free ctest `mesh_import` (`tests/mesh_import_test.cpp`), with fixtures
  in `tests/data/meshes/`:
  - `cube.obj`: 24 vertices, quads, written as `v`, `v//n`, `v/t`, `v/t/n` and negative
    indices;
  - `cube_binary.ply`: 24 vertices, quads;
  - `tetra_ascii.ply`;
  - `cube_ascii.stl`;
  - `cube_binary.stl`, whose header starts with `solid`.

  The fixtures were written once by a Python one-off, which is not kept. They are small
  enough to read or diff.
- **Docs:** new `docs/design/07-mesh-import.md`, listed in the design README; the roadmap
  focus.

## Acceptance criteria

| Criterion | Evidence |
|---|---|
| A ctest loads one small fixture per format, and checks the vertex and face counts after welding | `mesh_import`: OBJ, binary PLY and ASCII STL read 24/24/36 vertices and weld to 8 vertices and 12 triangles; ASCII PLY reads 4 and 4. Binary STL reads 36 and welds to 8 and 12. All three formats give the same ±10 mm cube. The weld is also checked on a soup: 9 vertices give 4, 3 triangles give 2, and 1 collapses. |
| A malformed file gives an error message, not a crash | 24 malformed inputs, each with its expected message checked. **OBJ:** empty, an index out of range, a word for a number or for an index, a two-vertex face, a relative index before the first vertex, 1e39, every triangle collapsing, binary junk. **PLY:** empty, no `end_header`, 4 000 000 000 vertices declared, an index out of range, a negative index, no face element, a binary file cut short. **STL:** empty, a binary file truncated, a count of 2³²−1, no triangles, an ASCII facet with 2 vertices, a word for a number, cut short, no facets. Also a missing file and a `.3mf`. |
| Register each mesh with `InMemoryMeshResolver` by id; the graph holds no path | `mesh_import`: `mesh(id=…)` builds the cube's distance (−10 at the centre, +10 10 mm outside); an unknown id is a `GraphError`. A scene of `mesh(id=…)` and `winding(id=…)` lowers. The object's graph has no `path` and the shader names none. |
| `python scripts/check.py` exits 0 | Exit 0 on the dev box, 17/17 ctests. |

The plan's criteria were right as written. The plan file is unchanged.

## Measured

No performance numbers. T1 has no performance criterion, and no large mesh has been read
or timed. Reading holds the file and the soup in memory, and the weld is one hash-map pass.

## What I got wrong on the way

- **I assumed the codegen took `id`.** `field_graph.h` documents `id=` sources and
  `InMemoryMeshResolver`, so I wired `resolveId` and expected lowering to work. The first
  run of the test aborted in `lowerScene`: `emitMesh` reads only `path`. The adapter above
  is the fix.
- A test opened a fixture with `QFile::open` unchecked (a `nodiscard` warning). It is now
  a check.
- The first message for a bad relative OBJ index read "vertex index 0 is out of range",
  which names an index the file does not hold. It now reads "relative index -4 reaches
  before the first vertex". A negative PLY index reads "a negative vertex index".

## Open

- **A question for the DualC owner:** should `emitMesh` and `emitWinding` in
  `examples/field_glsl.cpp` resolve `id=` through `resolver->resolveId`, as
  `FieldGraph::build` does? Then `meshIdsAsPaths` and `kMeshIdPath` can go. Until then the
  adapter costs nothing measurable: one graph copy per lowered object.
- **`.dcscene` does not carry library meshes yet.** `scene_file` embeds only meshes named
  by `path`, and leaves an `id` alone. A scene holding an imported mesh saves, but loads
  only in the process that imported it. T2 makes such objects, so T2 (or the unit that
  first saves them) must write the buffers in the archive and add them to the library on
  load. The design page § Not yet says so.
- The weld has no tolerance. A mesh whose shared corners differ in the last bit stays
  unwelded: it is then not watertight, and T2's diagnostics would make it a `winding`
  object.
- 3MF stays deferred (E-09).

## For the next unit (P04 T2)

- Drop path: `isMeshFileName(path)` tells a mesh file, and `readMeshFile(path)` gives the
  buffers or the message for the status bar. `addLibraryMesh(*read.mesh)` gives the id.
  The node is `GraphNode{op = "mesh" or "winding", params["id"] = String id}`;
  `tests/mesh_import_test.cpp` `meshNode` builds one.
- For DualC's `Diagnostics`, build the field or the mesh through `GraphDirMeshResolver`
  (`resolveId`), or register the buffers with your own `InMemoryMeshResolver`.
- `MeshRead::collapsed` and the welded counts can go in the object panel or the status
  line.
- Every object still lowers as `transform(scale(graph))`, so an imported mesh gets a pose
  with no new code.

## Human check

None owed. The change is GL-free readers and a resolver hook, checked by ctest; nothing
reaches the screen until T2's drop.
