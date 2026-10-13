# Handoff — P03 T2: save and load `.dcscene`

Date: 2026-10-13. Plan: `plan/40_P03_scene_model.md` § T2. DualC pin: `2fcb6ee` (unchanged).

**Status: done.** A scene saves to a `.dcscene` (E-08's zip container) and opens again.
The format is in `docs/design/05-dcscene-format.md`. `python scripts/check.py` exits 0 on
the dev box (NV92), with 15/15 ctests. A GTX check is owed (§ GTX check owed).

## What changed and why

- **`src/scene_file.{h,cpp}` (new).**
  - `saveSceneFile(scene, path)` writes a zip: `scene.json` plus `meshes/<n>.<ext>`, the
    bytes of every mesh file a graph reads. It uses miniz, which DualC already vendors and
    links through `dualc_examples_io`. The file is written with `QSaveFile`, whole or not
    at all.
  - `loadSceneFile(path)` checks `format` and then `version` before anything else. It
    refuses a non-zip, a missing version or any version other than 1, each with a message.
    It extracts the listed mesh entries to a per-load folder under one temp folder per
    process, which is removed at exit, and makes that folder every object's `meshDir`. It
    puts each object back with its id.
  - `writeZip` and `readZip` are public for the tests.
- **No absolute path in the file.**
  - An object's `file` is written relative to the scene's folder (`../src/a.fld`). On load
    it is kept only if it still exists there.
  - Every mesh `path` is rewritten to its entry name. Paths are resolved as the lowering
    resolves them (`meshDir` first). This includes the absolute paths `operationGraph`
    writes for operands from different folders.
  - A mesh read twice is stored once, keyed by SHA-256 and suffix.
- **The graph is written in the editor's own node form, not DualC's `dumpJson`.** Parameter
  edits are keyed by node pointer, and `dumpJson`/`parseJson` do not keep pointers: a
  shorthand graph's pointers are character offsets. Numbers use Qt's shortest round-trip
  form, so values come back bit-exact (the test compares field values with `==`).
- **`SceneDocument::replaceGraph(id, graph, meshDir = {})`.** A reload can now replace the
  object's `meshDir`, and undo restores it. A loaded object resolves meshes in the
  extracted folder, but its file's meshes sit beside the file. `MainWindow::fileEvent`
  passes `graphMeshDir(file)`. An empty `meshDir` keeps the old one, so T1's callers are
  unchanged.
- **Window (`MainWindow`):**
  - `openScene` replaces the scene through `SceneDocument::reset` (no history) with
    `next_ = {frame, loaded, attempt, opened}`, as `openFile` does. It records the text of
    each object's file, so only a later change reloads it.
  - `openFile` routes `.dcscene` to `openScene`, and `addFile` refuses one.
  - `saveScene` writes `document_.scene()`, the latest.
  - New menu entries: `File ▸ Save Scene` (`QKeySequence::Save`) and
    `Save Scene As…` (`QKeySequence::SaveAs`).
  - The Open dialog filters on `*.dcscene *.json *.fld`.
  - A drop of a `.dcscene` opens it, and any graphs in the same drop are added after it.
- **`main.cpp`:** a `.dcscene` argument opens in the window. `--snapshot` and `--bench`
  draw it when it is the only argument (`lowerGraphs`).
- **Tests:**
  - `tests/scene_file_test.cpp` (new ctest `scene_file`, GL-free). It covers each
    acceptance criterion; see `05-dcscene-format.md` § Tested by.
  - `tests/main_window_test.cpp` gains a block. It saves from the window and opens the
    file, and checks the same ids, names and flags, the same source, the same uniform
    values, and an empty undo stack. It also checks that a drop opens the file,
    `--snapshot` of it exits 0 (or skips), and a non-zip is refused, with "not a scene
    file" in the status bar and the scene kept.
- **Docs:**
  - `docs/design/05-dcscene-format.md` (new) and the design README.
  - `04-scene-model.md`: edits now reach the `.dcscene`, and `ReplaceGraph` carries
    `meshDir`.
  - The command reference: § Scene files, the File menu, Open, the drop and the
    positional argument.
  - E-08 is marked implemented in `02-open.md`, and the roadmap focus moves to T3.

## Acceptance criteria

| Criterion | Where |
|---|---|
| A save then load reproduces the scene. Field values at sample points, and mesh buffers byte for byte. | `scene_file`: 343 points per object compared exactly. The mesh entry, the extracted file and the baked `MeshTexture::values` are compared byte for byte, along with the lowered source and the uniform values. |
| A future `version` is refused with a message. | `scene_file`: "future.dcscene is scene version 2, written by a newer DualCEditor; this DualCEditor reads version 1". |
| Moving the file to another directory still loads it. | `scene_file`: the file is copied to `b/c/moved.dcscene`, and its source graphs, mesh and first folder are deleted. It loads with the same fields, source, values and mesh textures, and saves again. |

The plan's criteria were right as written. The plan file is unchanged.

## Measured

There are no performance numbers. This unit adds no GPU work, and the load compiles as any
open does. Gate on the dev box (NV92), Release: 15/15 ctests. `scene_file` ran in 1.6 s and
`main_window` in 22.7 s, new block included.

## What I got wrong on the way

Nothing failed after the code was first written. Two design turns are worth recording:
- I first meant to write graphs with DualC's `dumpJson`, so a graph could be lifted out and
  run with `dualc_field`. It loses node pointers, which parameter edits are keyed by.
  Rejected: see above and the design page.
- I first meant to extract meshes under their original relative paths. A path like
  `../meshes/x.obj` would then write outside the extraction folder, and absolute paths
  would have no place there. Rejected: paths are rewritten to `meshes/<n>.<ext>`, and the
  reader accepts only plain names under `meshes/`.

## Open

- **No "unsaved changes" prompt** and no dirty mark in the title. Quitting or opening
  discards edits silently, as it did before T2. The undo stack's `cleanChanged` is the hook
  for it. Nothing in the plan asks for it yet.
- **No `grids/`.** E-08's optional baked SDFs are not written, and meshes are baked again at
  load.
- **`mesh(id=…)` nodes are written as they are.** P04 (in-memory meshes, E-09) must decide
  how they are embedded. The natural place is the same `meshes/` entries, with the id kept.
- **Extracted meshes live in the temp folder until exit.** Each load makes a new subfolder.
  Many loads of big meshes in one session use disk.
- **A loaded object whose file has changed since the save** shows the saved graph until
  the file changes again. This is the same rule as an undone reload (T1).

## For the next unit (P03 T3, gizmo)

- The transform is saved as `worldFromLocal`, row-major. T3's criterion "saves and reloads
  at the same pose" is `saveSceneFile` then `loadSceneFile`, with `scene_file_test`'s
  `sameObjects` and `samples` as ready-made checks. `samples` evaluates each object in the
  world through DualC's `transform` node.
- A rotation is already in the test (`Mat4::rotation` about z, 0.3 rad), and it
  round-trips exactly.

## GTX check owed

On the GTX 1050 Ti laptop, a Release build of this commit:
1. `ctest --test-dir build --output-on-failure`. Expected: 15 tests, with `scene_file`
   passing. `main_window` keeps its known failure (".fld renders a different image"). Any
   failure in its new scene-file block is news.
2. `DualCEditor a.fld b.fld` (any two graphs; one with a mesh, e.g. DualC's
   `mesh_lattice.json` with a `cube.obj` beside it). Then:
   - Move one object (Parameters ▸ matrix), hide the other, recolour one and edit one
     parameter.
   - `Ctrl+S`. A save dialog opens in the graph's folder, named `<graph>.dcscene`. Save.
     The status bar says `Saved …: 2 objects`.
   - Copy the `.dcscene` to another folder, e.g. the desktop, and quit.
   - `DualCEditor <desktop>/<name>.dcscene`. The scene shows as it was saved: pose, hidden
     object, colour and the edited parameter. **Is the image the same as before the
     save?** NV92 cannot judge images across a relink.
   - `Ctrl+Z` does nothing: there is no history after an open.
   - Drag the `.dcscene` onto the window: it opens again.
   - With AZERTY: `Ctrl+S` saves and `Ctrl+Shift+S` asks for a name. On Windows, check
     that the save dialog adds `.dcscene`.
3. Edit the original graph file in a text editor and save. When the scene was opened from
   its own folder, the object reloads; from the copy elsewhere, nothing reloads.
