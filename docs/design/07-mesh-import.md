# 07 — Mesh import: readers, welding, meshes by id

How a mesh file becomes triangles a graph can name. Born in P04 T1 (2026-10-13).
Implements E-09 (`../decisions/02-open.md`): OBJ, PLY and STL, with 3MF deferred. The drop
and the `mesh`/`winding` choice are P04 T2's, and are not built yet.

## Readers

`src/mesh_import.{h,cpp}`. `readMeshFile(path)` picks the format by suffix (`.obj`,
`.ply`, `.stl`, in any case: `meshFormatFor`, `isMeshFileName`). `readMeshBytes(bytes,
format, name)` reads bytes already in memory. Both return a `MeshRead`: the welded
`MeshBuffers` (3 floats per vertex, 3 `uint32` indices per triangle), or an `error`. It also
carries what the weld did (`readVertices`, `readTriangles`, `collapsed`).

| Format | Reader | What it reads |
|---|---|---|
| OBJ | own | `v x y z` and `f`, with each corner written `v`, `v/t`, `v//n` or `v/t/n`. Indices are 1-based, or negative (relative to the vertices so far). Every other statement (`vt`, `vn`, `o`, `g`, `s`, `usemtl`, `l`, …) is skipped, and `#` starts a comment. |
| PLY | geometry-central (`SimplePolygonMesh`, through happly) | ASCII and binary. `vertex` x y z, and `face` `vertex_indices` (or `vertex_index`). |
| STL | own | Binary and ASCII. The vertex order gives the orientation; the stored normal is not read, since writers often leave it zero. Several `solid`s in one file are read as one. |

**Why own OBJ and STL readers.** geometry-central reads both, but the pinned copy:
- reads a word in an OBJ vertex line as 0, with no error;
- trusts a binary STL's triangle count. Reading past the end gives garbage, and a count of
  billions allocates without bound.

E-09 asked for an own STL reader. OBJ got one too, for the same reason: a malformed file
must give a message.

**Binary or ASCII STL.** A file is ASCII when it starts with `solid`, unless its size is
exactly `84 + 50 × n`, where `n` is the count at byte 80. Many binary writers also start
their header with `solid` (`tests/data/meshes/cube_binary.stl` does).

**Numbers.** Numbers are parsed with `QByteArrayView::toDouble`, which always reads the C
locale. `strtod` would follow the user's locale: on the owner's Belgian locale it reads
`0,5`, not `0.5`.

**Malformed files.** Every check ends in a message that starts with the file name and says
where the fault is: `cube.obj: line 4: vertex index 4 is out of range (3 vertices)`, or
`facet 1: a facet needs 3 vertices, it has 2`. There is no crash, and no exception leaves
`readMeshBytes`. The checks:
- every index is in range, and none is negative;
- every face has at least 3 vertices;
- every coordinate is a finite float (1e39 is not);
- a binary STL is as long as its count says.

Before happly sees a PLY, `checkPlyHeader` checks three things: the `ply` magic, an
`end_header`, and that the elements declared fit in the bytes after the header. Each element
takes at least one byte in any encoding, so a count of billions is refused before anything
is allocated. happly does not check its binary reads. A binary PLY cut short can therefore
still read, as zeros, and the index check refuses it when it does
(`mesh_import_test` "a binary file cut short returns").

## Welding

`weldVertices(vertices, indices)`. Polygons are fanned into triangles first: `(0, k, k+1)`.
Then:
- **Which vertices weld.** Two vertices weld when their float coordinates are identical,
  with `-0` read as `0`. There is no tolerance. STL writes each triangle's corners
  separately, but from the same floats, so exact matching joins them, and it never merges
  two vertices that a model keeps apart on purpose.
- **Numbering.** Vertices are numbered in the order they are first used. A vertex no
  triangle uses is dropped.
- **Collapsed triangles.** A triangle left with a repeated vertex is dropped and counted in
  `collapsed`. geometry-central's `SurfaceMesh` cannot hold such a face.
- **Nothing left.** A mesh with no triangle left is an error.

The fixtures' cubes weld from 24 (OBJ, PLY) and 36 (STL) vertices to 8, with 12 triangles.

## Meshes by id

`src/mesh_library.{h,cpp}`. A read mesh goes into one library per process:
- `addLibraryMesh(buffers)` returns its id: `mesh-` and the first 16 hex digits of the
  SHA-256 of the counts and the buffers. The same buffers give the same id, so a mesh
  imported twice is held once.
- `findLibraryMesh(id)` gives it back.
- The library takes a mutex, since the compile thread (`01-render-path.md` § Asynchronous
  compile) resolves ids while the window imports.
- A mesh is never removed, so an undone import can be redone.

A graph names the mesh as `mesh(id="mesh-…")` or `winding(id="mesh-…")`, never by a file
path. `GraphDirMeshResolver::resolveId` (`src/graph_file`) finds the id in the library and
registers it with DualC's `InMemoryMeshResolver` on first use, once per resolver.
Lowering (`lowerScene`) and picking (`ScenePicker`) build a scene object's field through
that resolver, so an object's id resolves wherever its path would. A graph *file* naming an
id does not compile: `compileGraphFileText` has no adapter (below), and the id is not in a
fresh process's library anyway.

**The codegen's `path` adapter.** DualC's GLSL codegen at the pinned commit (`emitMesh`
and `emitWinding` in `examples/field_glsl.cpp`) reads only `path`, and refuses `id`
("'mesh' requires a 'path'"). `FieldGraph::build` takes both. So `lowerScene` hands the
codegen a copy of the graph (`meshIdsAsPaths`), with each id rewritten as the path
`dualceditor-mesh-id:<id>`. `GraphDirMeshResolver::resolve` sends that path back to
`resolveId`. The scene's graph keeps the id; only the codegen's copy carries the made-up
path, and no file is written. When DualC's codegen takes `id`, the adapter goes: this is a
question for the owner (DualC).

## Not yet

- **Saving.** A `.dcscene` (`05-dcscene-format.md`) leaves an `id` source alone and embeds
  only files named by `path`. A scene holding an imported mesh therefore saves, but loads
  only in the process that imported the mesh. The buffers must go in the archive, which is
  for P04 T2, when the drop makes such objects.
- **3MF** is deferred (E-09).
- **Size.** Reading and welding hold the whole file and the soup in memory. No large mesh
  has been timed.
