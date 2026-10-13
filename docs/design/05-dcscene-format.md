# 05 — The `.dcscene` file

How a scene is saved and opened again. Born in P03 T2 (2026-10-13). Implements E-08, the
zip container (`../decisions/02-open.md`). The scene it saves is `04-scene-model.md`'s.

## The container

`src/scene_file.{h,cpp}`. A `.dcscene` is a zip archive, written and read with miniz, which
DualC already vendors (MIT, `examples/third_party/miniz.{h,c}`, linked through
`dualc_examples_io`). The editor vendors nothing (E-01). It holds:

| Entry | Content |
|---|---|
| `scene.json` | The scene: every object, in scene order. |
| `meshes/<n>.<ext>` | The bytes of each mesh file a graph reads, as they were on disk. `<n>` counts from 0; `<ext>` is the file's own suffix, lower case (`obj`, `ply`). |

E-08 named the mesh entries `meshes/<n>.bin`. They keep their file's suffix instead, so
DualC's `FileMeshResolver`, which picks a reader by suffix, reads the extracted file as it
read the original. A mesh read by several nodes or objects (an operation copies its
operands' graphs) is stored once: entries are keyed by the SHA-256 of their bytes and their
suffix. Baked SDF grids (E-08's optional `grids/`) are not written: the mesh is baked again
at load, as at any compile.

The file is written whole or not at all (`QSaveFile`).

## `scene.json`

```json
{
  "format": "dcscene",
  "version": 1,
  "meshes": ["meshes/0.obj"],
  "objects": [
    {
      "id": 2,
      "name": "mesh.fld",
      "file": "../src/mesh.fld",
      "visible": false,
      "colour": [0.1, 0.2, 0.3],
      "transform": [1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1],
      "operands": [],
      "edits": [{"node": "/root/in/1", "param": "radius", "value": [1.1]}],
      "graph": {"op": "intersection", "in": [ ... ], "pointer": "0"}
    }
  ]
}
```

| Key | Meaning |
|---|---|
| `format` | Always `dcscene`. |
| `version` | `1` (`kSceneFileVersion`). Read before anything else. |
| `meshes` | The mesh entries the archive holds. Only these are extracted. |
| `objects[].id` | The object's id, kept: its uniforms are named after it, and `operands` refer to it. |
| `objects[].file` | The graph file the object was read from, **relative to the `.dcscene`'s folder** (`../src/a.fld`). Left out for an object with no file (an operation's), or when no relative name exists (another drive on Windows). A Qt resource keeps its `:/` name. |
| `objects[].transform` | `worldFromLocal`, 16 numbers, row-major (`dualc::Mat4::m`). |
| `objects[].scale` | The uniform scale under the transform (P03 T3), a positive number. Optional: absent reads 1. Anything else is refused. |
| `objects[].colour` | Linear RGB, as `SceneColour`. |
| `objects[].operands` | The ids an operation made the object from (`04-scene-model.md`). |
| `objects[].edits` | The parameter edits: the binding's node pointer, its param key and the uniform's value. |
| `objects[].graph` | The field graph, one node per object: `op`, `params` (number, string or array of numbers), `in`, `pointer`. Every mesh `path` is the name of its entry (`meshes/0.obj`). |

### Why the graph is not DualC's canonical JSON

DualC's `dumpJson` writes the canonical form, which `parseJson` reads back. It does not
keep node pointers: a reparse gives `/root/in/0`-style ones. A parameter edit is keyed by
its node's pointer, and a shorthand graph's pointers are character offsets
(`04-scene-model.md` § Operations). So the scene writes its own node form, pointers
included, and an edit finds its node again. The node form is DualC's tree as the editor
holds it (`dce::fieldgraph::GraphNode`), with numbers written in Qt's shortest round-trip
form, so a value read back is the same `double`.

## No absolute path

The file names nothing by an absolute path. An object's file is relative to the scene's
folder. A mesh path is the name of the zip entry: when the scene is saved, each mesh path is
resolved as the lowering resolves it (beside the object's `meshDir` first, then as given,
`GraphDirMeshResolver`), read and stored, and rewritten to its entry name. This holds for
the absolute paths `operationGraph` writes when operands come from different folders.

A `mesh(id=…)` node (DualC's in-memory meshes) is written as it is. P04 brings those ids
to the editor.

## Reading

`loadSceneFile`:
1. Not a zip, no `scene.json`, or no `"format": "dcscene"`: refused, `"<name> is not a
   scene file: …"`.
2. `version` missing: refused, `"<name> has no scene version; this DualCEditor reads
   version 1"`. A version other than 1: refused, `"<name> is scene version 2, written by a
   newer DualCEditor; this DualCEditor reads version 1"`. DualC's own graph reader ignores
   its `version`; this one does not.
3. The `meshes` entries are extracted to a folder made for this load, under one temporary
   folder per process (`DualCEditor-XXXXXX` in the system temp folder, removed at exit).
   An entry must be `meshes/<plain file name>`, with no `/`, `\`, `:` or `..`, and be in the
   archive; anything else is refused. That folder becomes each object's `meshDir`, so the
   entry names in the graphs resolve to the extracted files.
4. Each object is put back with its id (`Scene::insert`). An object's `file` is kept when
   it exists relative to the scene's folder, and dropped otherwise: the graph in the scene
   file is what loads, so a scene moved to another folder, or whose sources were deleted,
   loads the same scene.

A loaded object's `meshDir` is the extracted folder; its graph file's meshes sit beside the
file. So a reload from that file replaces the `meshDir` too
(`SceneDocument::replaceGraph(id, graph, meshDir)`), and an undo of the reload puts the old
one back.

## In the window

`MainWindow::openScene` replaces the scene with the loaded one, with no undo history
(`SceneDocument::reset`, as `File ▸ Open` of a graph), and frames it. Each object's file
still on disk is watched. Its text is recorded at the open, so only a later change reloads
it: until then the scene file's graph, with its parameter edits, is what shows, even when
the file on disk has changed since the save.

`MainWindow::saveScene` writes the document's scene, the latest, which may run ahead of
the one on screen while a scene compiles. The keys and menus are in
`../command_reference/01-viewport.md` § Scene files.

## Tested by

`tests/scene_file_test.cpp`, ctest `scene_file`, GL-free. It builds a scene with a JSON
graph, a shorthand graph reading a mesh beside it, a sphere and a union over the last two,
with a rename, two moves (one rotated), a hidden object, a colour and a parameter edit. It
saves the scene and loads it back, then checks:
- every object field, and the graphs with their mesh paths blanked;
- each object's field values at 343 points over its world box, compared exactly;
- the lowered source and every uniform value;
- the baked mesh textures, and the mesh entry and the extracted file against the original
  file, byte for byte.

It then moves the file to another folder, deletes the sources, and loads it again. It
also checks that a future version, a missing version, a non-zip and a mesh entry outside
`meshes/` are refused. `tests/main_window_test.cpp` saves from the window, opens the
file, drops it, and draws it with `--snapshot`.
