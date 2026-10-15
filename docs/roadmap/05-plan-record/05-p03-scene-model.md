# P03 — Scene model, undo, `.dcscene`

**DONE (2026-10-13).** Waited on E-08 (scene file format; decided 2026-10-13 by delegation:
the zip container). From here on an operation produces a new object whose graph refers to
its operands; the scene keeps that history and saving keeps it too. Design:
`../../design/04-scene-model.md`, `../../design/05-dcscene-format.md`,
`../../design/06-transform-gizmo.md`. Usage: `../../command_reference/01-viewport.md`
§ Undo and redo, § Scene files, § Transform. GTX checks owed:
`../04-gtx-batch-2026-10-13.md` § 2, § 3, § 4. Development-box conditions: `README.md`.

## T1. Operation history and undo

*Planned:* every change goes through a command on a `QUndoStack`: adding, removing and
renaming an object, transforms, visibility, parameter edits, operations; an operation
hides its operands and does not delete them; the model in `../../design/04-scene-model.md`, closing
E-25 (the plan said `03-…`; `03` was taken, pages number by birth). Acceptance: undo and
redo restore the exact scene (the same graphs, the same visibility); a ctest does a round
trip through a sequence of commands without GL.

**DONE (2026-10-13, commit b95ee25).** Delivered: `SceneDocument` (`src/scene_document`)
holding the `Scene` and a `QUndoStack`, one command per edit: `AddObject` / `RemoveObject`
(keep the object, its id and index), `SetField<name|transform|visible|colour>`,
`SetParameter` (merges consecutive edits of one parameter; an edit dragged back to its
start leaves no step), `ReplaceGraph` (a file reload), `ApplyOperation` (adds the result,
hides the operands that were shown), `beginMacro` / `endMacro` (a reload of several files
is one step), `reset` (File ▸ Open: a new document, no history); one `changed(Changes)`
(params, objects, structure) at the end of each push, undo, redo, merge or macro; no GL.
`operationGraph`: the op node over each operand's graph, each wrapped in `transform` when
moved; every node gets a structural pointer because operands can share `/root` or shorthand
offsets; the operands' edits are re-keyed; relative mesh paths become absolute when the
operands' `meshDir`s differ. `SceneObject` gained `file`, `edits` (parameter edits as
uniform values keyed by `(nodePointer, paramKey)`, not written into the graph because a
uniform is not always a param: `angleRad`, defaults, the `mat4` of `translate`/`rotate`)
and `operands`; `lowerScene` applies the edits; `Scene::insert`, `indexOf`, `nextId`.
`MainWindow` edits only through the document: `documentChanged` sets uniforms at once
(`syncObject`: visible, colour, matrix, culling box) or calls `apply` for a structural
change; the dock's `matrix`, `visible` and `colour` edits become document commands (before
this a `matrix` edit never reached `SceneObject::transform`); `revertDocument` undoes or
redoes back to `shownIndex_` when a scene cannot be shown. UI: `Edit ▸ Undo` / `Redo`
(`QKeySequence::Undo` / `Redo`), `Edit ▸ Rename Object` (`F2`), editable Objects rows. No
operation UI yet (P05). Tests: `scene_history` (GL-free: 11 commands, 3 adds, rename,
move, hide, recolour, a merged parameter edit, union, reload, remove; undone to the empty
scene and redone to the end; at every step `sameScene`, every field and graphs node for
node, and `lowerScene` gives the same source and uniform values; merging, the obsolete
edit, ids not reused, macros, reset); `main_window` gains hide all, undo restores the image
with no link, redo, rename reaches the row, remove then undo gives the same source and
uniform values. E-25 closed. 14/14 ctests.
- *Wrong on the way:* a merged parameter edit that became obsolete left an entry equal to
  the default, because `QUndoStack` drops an obsolete command without undoing it;
  `mergeWith` now undoes it itself (`scene_history` caught it). `sameScene` first compared
  `nextId`, which an undone add keeps advanced by design. The window test's first hide
  picked the front of two objects and the image did not change because the other covers
  it; it hides every object. "Undo of a remove restores the image" failed on NV92 with
  identical source and uniforms but 134 268 px differing (the pre-remove frame was drawn
  oddly, a cyan background); the check compares source and uniforms, and whether the image
  matches across a relink is the GTX question (§ 2 of the batch).
- *Open:* an operation's result is a copy of its operands' graphs (E-24 stays deferred),
  so editing an operand afterwards does not change the result. Parameter edits do not reach
  the graph file; they live in `SceneObject::edits` (and the `.dcscene`, T2). The NV92
  image difference after a relink is unexplained and may be the dev box's driver. A reload
  is an undo step: undoing it shows the old graph while the file holds the new text, not
  re-read until it changes again.

## T2. Save and load `.dcscene`

*Planned:* E-08's format with a `version` the reader checks, embedded meshes, relative
names and never an absolute path; documented in `../../design/05-dcscene-format.md`. Acceptance: a
save then load reproduces the scene (field values at sample points, mesh buffers byte for
byte); a future `version` is refused with a message; moving the file elsewhere still
loads it.

**DONE (2026-10-13, commit 8a44196).** Delivered: `src/scene_file`: `saveSceneFile` writes
a zip (`scene.json` plus `meshes/<n>.<ext>`, the bytes of every mesh file a graph reads)
through miniz, which DualC vendors and links through `dualc_examples_io`, with `QSaveFile`
(whole or not at all); `loadSceneFile` checks `format` then `version` first, refuses a
non-zip, a missing version or any version other than 1 with a message, extracts the listed
mesh entries to a per-load folder under one temp folder per process (removed at exit) and
makes it every object's `meshDir`, puts each object back with its id; `writeZip` /
`readZip` public for tests. No absolute path: an object's `file` is written relative to the
scene's folder and kept on load only if it still exists there; every mesh `path` is
rewritten to its entry name (including the absolute paths `operationGraph` writes); a mesh
read twice is stored once, keyed by SHA-256 and suffix. The graph is written in the
editor's own node form, not DualC's `dumpJson`, because edits are keyed by node pointer
and `dumpJson` / `parseJson` do not keep pointers; numbers in Qt's shortest round-trip
form, bit-exact. `SceneDocument::replaceGraph(id, graph, meshDir = {})`, so a reload can
replace `meshDir` and undo restores it. Window: `openScene` through `reset` (no history);
`openFile` routes `.dcscene` there and `addFile` refuses one; `saveScene` writes the latest
document; `File ▸ Save Scene` (`QKeySequence::Save`) and `Save Scene As…`; the Open
dialog filters `*.dcscene *.json *.fld`; a dropped `.dcscene` opens, graphs in the same
drop are added after it; a `.dcscene` argument opens, and `--snapshot` / `--bench` draw it
when it is the only argument. Tests: `scene_file` (GL-free) and a `main_window` block
(save from the window and open: same ids, names, flags, source, uniform values, empty undo
stack; a drop opens; `--snapshot` of it exits 0 or skips; a non-zip is refused with "not a
scene file" and the scene kept). E-08 marked implemented. 15/15 ctests (`scene_file` 1.6 s,
`main_window` 22.7 s).
Verified against the criteria: 343 points per object compared exactly after a round trip;
the mesh entry, the extracted file and the baked `MeshTexture::values` byte for byte, plus
the lowered source and uniforms; the message "future.dcscene is scene version 2, written
by a newer DualCEditor; this DualCEditor reads version 1"; the file copied to
`b/c/moved.dcscene` with its source graphs, mesh and first folder deleted loads with the
same fields, source, values and textures, and saves again.
- *Rejected on the way:* writing graphs with DualC's `dumpJson`, so a graph could be
  lifted out and run with `dualc_field`: it loses node pointers (a shorthand graph's are
  character offsets). Extracting meshes under their original relative paths: `../meshes/x.obj`
  would write outside the extraction folder and absolute paths would have no place; paths
  are rewritten and the reader accepts only plain names under `meshes/`.
- *Open:* no "unsaved changes" prompt and no dirty mark in the title; quitting or opening
  discards edits silently (`cleanChanged` is the hook; nothing in the plan asked for it).
  No `grids/`: E-08's optional baked SDFs are not written, meshes are baked again at load.
  Extracted meshes live in the temp folder until exit, a new subfolder per load. A loaded
  object whose file changed since the save shows the saved graph until the file changes
  again (the same rule as an undone reload). `mesh(id=…)` nodes were written as they were
  until P04 T2 embedded library meshes.

## T3. Transform gizmo

*Planned:* move, rotate and scale the selected object from the viewport and from a numeric
panel in millimetres, typed in as DualC's graph `units` are. Acceptance: a transformed
object renders, saves and reloads at the same pose; a moved object is still drawn whole
with no compile, the trace box (`uBoundsMin`, `uBoundsMax`) and the culling box (P07 T1)
following as uniforms (`../03-go-no-go.md` item 2); the command reference is updated.

**DONE (2026-10-13, commit 320b946).** Delivered: the scale is a uniform factor
`SceneObject::scale` beside the rigid `transform`: DualC's `transform` inverts with
`inverseRigid`, so a scale cannot go in the matrix, and DualC's `scale` node (`s * f(p/s)`)
keeps the field a distance only for a uniform scale, so there is no per-axis scale (the
plan said "scale" without settling it; the option that keeps the field exact was taken).
`lowerScene` wraps every object as `transform(scale(graph))`, always, so a scale edit is
the uniform `s` (`LoweredObject::scaleBinding`); the two wrappers share the pointer
`/objects/<id>`; `operationGraph` writes `scale` only when it is not 1; the picker divides
by the scale; `worldBox(local, transform, scale)`; `.dcscene` gains `objects[].scale`,
optional, 1 when absent, a non-positive value refused, version still 1.
`SceneDocument::setPose`, one `SetPose` command named `Move|Rotate|Scale|Transform <name>`.
The trace box follows: `sceneTraceBounds(scene, lowered)`, `TraceRenderer::setBounds`,
`Viewport::setTraceBounds`, `MainWindow::pushUniforms` (matrix, scale, culling box, trace
box). Gizmo: `src/gizmo` GL-free (`Pose`, `PoseValues` as XYZ Euler degrees,
`projectPoint`, `GizmoDrag`); `Viewport` runs the modal drag, draws the axes with
`QPainter` after the frame, emits `gizmoDragged` / `gizmoFinished`; the window previews a
drag into the scene on screen only and pushes the pose once at the end. Transform dock
(`src/transform_panel`): position in mm, rotation in degrees, the scale; boxes do not track
keystrokes, so a typed value is one undo step and an untyped component keeps its exact
value. Tests: `gizmo` (GL-free), a `main_window` gizmo block, `scene_file` with the ball
scaled 1.5. 16/16 ctests.
Verified: `scene_file`, the gyroid moved and rotated, the ball moved and scaled 1.5:
`sameObjects` (matrix memcmp, scale `==`) and 343 field samples per object equal after save
and load. `main_window` on NV92: the dock moves the gyroid 3 box widths, wholly outside its
old trace box, `F` frames it and it covers as many pixels as before within 2% (before T3,
F framed the old box and the object covered 0 px); `vp.scene().bounds.min.x > old max.x`;
`linkCount` and `compileCount` unchanged across a drag, a typed move and a scale. `gizmo`:
`sceneTraceBounds` after a move equals a fresh `lowerScene`'s bounds; the culling box is
`worldBox(local, transform, scale)`; undo and redo of a pose. Two costs added to every
lowered object: a `scale` wrapper (one divide and one multiply per evaluation) and a
`float` uniform; the GTX check (§ 4 of the batch) times them against P07's numbers.
- *Wrong on the way:* the window test used `QUndoStack::count()`, which includes undone
  steps (the block hides and un-hides to count pixels); `index()`. A typed x with more than
  3 decimals was rounded by the box; whole millimetres now. The sphere parameter is
  `radius`, not `r`.
- *Open:* **`featureScale` does not follow a scale**: the step bounds come from the trace
  box (which follows) and from `featureScale` (as lowered), so a large scale-down without
  a recompile could step past thin features until the next structural change; the batch's
  § 4 step on scale 0.2 watches for it, and the fix is to scale `featureScale` with the
  smallest object scale in `pushUniforms`. No snapping during a drag (the dock gives exact
  values). The gizmo has no handles: key-driven (Blender style), axes drawn, nothing to
  drag directly. The `QPainter` overlay is not in the GPU frame timing (drawn after the
  query ends). A rotation typed in the dock goes through Euler angles: a gizmo pose shows
  its decomposition, which near gimbal lock (y = ±90°) puts the whole turn in z.

## Exit criteria

T1 to T3 met as above; `python scripts/check.py` green (16/16 ctests). The GTX checks of
all three tasks are queued (`../04-gtx-batch-2026-10-13.md` § 1–4).

---
← Back to the [Plan record](README.md) · [Roadmap index](../README.md).
