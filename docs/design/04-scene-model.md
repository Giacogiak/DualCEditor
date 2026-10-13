# 04 — Scene model and undo

How the scene is edited, and how every edit is undone. Born in P03 T1 (2026-10-13). Closes
E-25 (`../decisions/03-deferred.md`). The scene itself and its lowering are
`03-scene-lowering.md`.

## The document

`src/scene_document.{h,cpp}`. A `SceneDocument` holds the `Scene` being edited and a
`QUndoStack` (Qt GUI). Every change to the scene is one `QUndoCommand` pushed on that
stack; nothing edits the scene around it. It touches no GL, so the history is tested
without a context (`tests/scene_history_test.cpp`).

`SceneObject` (`scene.h`) gained three fields for it:

| Field | Meaning |
|---|---|
| `file` | The graph file the object was read from, watched for reloads; empty for an operation's object. It replaces the window's old id → file map, so an undone remove brings its watch back. |
| `edits` | Parameter edits: uniform values keyed by `(nodePointer, paramKey)`, the binding's own key. |
| `operands` | The ids an operation made the object from, in order; empty for a file's object. |

### Commands

| Edit | Command | Rebuild? |
|---|---|---|
| Add an object (File ▸ Add, a drop, a command-line graph after the first) | `AddObject`: the object, its id and its place | yes |
| Remove an object | `RemoveObject`: keeps the whole object and its place; undo puts it back with its id | yes |
| Rename | `SetField<name>` | no |
| Move (the `matrix` binding) | `SetField<transform>`: `worldFromLocal` | no: the transform and box uniforms |
| Move, rotate, scale (the gizmo, the Transform dock, the `s` binding; P03 T3) | `SetPose`: `transform` and `scale` in one step, named `<verb> <name>` | no: the transform, scale, culling box and trace box uniforms (`06-transform-gizmo.md`) |
| Show or hide | `SetField<visible>` | no: one uniform |
| Recolour | `SetField<colour>` | no: one uniform |
| Parameter edit | `SetParameter`: the edit before (or none) and the uniform's value before; consecutive edits of one parameter merge into one step | no: one uniform |
| A file reloaded | `ReplaceGraph`: the old graph, its edits and its `meshDir`; the new graph has none, and resolves meshes beside its file | yes |
| An operation | `ApplyOperation`: adds the result after its last operand and hides the operands that were shown | yes |

A reload of several files is one macro (`SceneDocument::beginMacro`), so one step and one
rebuild. `File ▸ Open` is not a command: `SceneDocument::reset` replaces the scene and
clears the history, as a new document.

Ids only grow (`Scene::nextId`). An undone add, redone, takes its id back; a new add after
an undo takes a new one. `Scene::insert` puts an object back at its index with its id.

### Why parameter edits sit beside the graph

A uniform is not always a graph parameter: DualC binds `angleRad` for an angle given in
degrees, binds defaults the graph never wrote, and a `mat4` for `translate` and `rotate`.
So an edit is kept as the uniform's value, keyed by the binding, and `lowerScene` puts it
into the lowered bindings (`applyEdits`). The graph stays as read. The graph file never
holds them; the `.dcscene` does (`05-dcscene-format.md`). A reload drops them, as before;
undoing the reload brings them back.

### Changes and the window

A command does not talk to the window. Its edits record what changed
(`touch`, `touchGraph`, `touchParam`), and the document sends one
`changed(Changes)` at the end of each push, undo, redo, merge or macro:
- `params`: one uniform each, with the value to show (on an undone edit, the value the
  uniform had before it, which is the graph's when there was no edit);
- `objects`: ids whose name, flag, colour or transform changed;
- `structure`: an object came or went, or a graph changed.

`MainWindow::documentChanged` sets the uniforms of the scene on screen at once
(`syncObject` writes `visible`, `colour`, the `matrix` binding as `localFromWorld` and the
culling box) and copies the fields into the scene on screen and the one compiling. A
structural change is lowered and linked off the GUI thread as before
(`01-render-path.md` § Asynchronous compile). How a load applies (frame the camera, which
file the status bar names, which files to watch on failure) is set by the caller in
`next_` before it edits the document; an undo or redo applies with the defaults.

The document runs ahead of the screen while a scene compiles. When the new scene cannot
be shown (a field-graph error, a shader that does not link), the window brings the
document back to the scene on screen: it undoes or redoes to the undo index recorded at
the last commit (`shownIndex_`), and the undone step stays on the redo side. After a
failed open it resets the document to the scene on screen, with no history.

## Operations

An operation makes a new object; its operands are hidden, not deleted, so undo shows them
again. `operationGraph` builds its graph: the operation's node over each operand's graph,
wrapped in DualC's `transform` with the operand's `worldFromLocal` (none for an identity).
The new object's own transform is the identity.

- **A copy, not a reference.** DualC's graph is a tree (DualC D-12) and references between
  objects are E-24, deferred. So "refers to its operands" is the `operands` list, the
  history; the graph holds copies. Editing an operand after the operation does not change
  the result.
- **Pointers.** Two operands can carry the same pointers (`/root`, or a shorthand's
  character offsets). Every node of the result gets a structural pointer (`/root`,
  `/root/in/0`, …), and the operands' edits are re-keyed to it, so they follow into the
  result.
- **Meshes.** The result takes its first operand's `meshDir`. When the operands resolve
  meshes in different folders, relative mesh paths that exist there become absolute.
  P04 moves meshes to `InMemoryMeshResolver` ids, which ends this.

- **Booleans (P05 T1).** DualC's booleans take two inputs. Over more operands
  `operationGraph` folds left in operand order, `op(op(a, b), c)`, every node with the
  same params (`k`); the pointers stay structural (operand 0 of three at
  `/root/in/0/in/0`).
- **Non-metric operands (E-10, decided: wrap automatically).** For a smooth boolean, an
  operand for which `isMetricGraph` is false goes under `normalize`, outermost, above its
  `transform` and `scale`. `isMetricGraph` is false for a graph holding a `winding`, a
  TPMS source, or a `twist`, `bend` or `displace`, unless a `normalize` sits above it; a
  `mesh` is a distance field and is not wrapped (wrapping it would cost `normalize`'s
  gradient samples for nothing). The undo step's text names the wrapped operands, which
  is the history E-10 asks for. Sharp booleans need no wrap: `min` and `max` keep the
  zero set of any field.
- **The window.** The `Operations` menu (`MainWindow::applyBoolean`) takes the Objects
  dock's selection in the order it was made (`ObjectsPanel::selectedIds`), and the menu's
  `k`. The result is selected when the scene that holds it commits.

`tests/operations_test.cpp` checks every boolean against DualC: the result's graph, read
back as `dualc_field` reads a graph (`dumpJson`, `parseJson`), has the same field at 216
points as the graph written by hand in DualC's shorthand. Both are built by DualC's
`FieldGraph`, so equal values mean the editor builds the graph a user would give
`dualc_field`; what the GLSL makes of it is DualC's own parity test (`dualc_glsl_parity`).

## The window's inputs

`Edit ▸ Undo` and `Edit ▸ Redo` are `QUndoStack::createUndoAction` and `createRedoAction`,
named after the step (`Undo Hide a.fld`), on `QKeySequence::Undo` and `Redo`. A rename is
made in the Objects dock (`F2`, a double click, `Edit ▸ Rename Object`): the row keeps the
old name until the document's change renames it, and an empty name is refused
(`../command_reference/01-viewport.md` § Objects).

The selection is not part of the scene: removing the selected object clears it, and its
undo does not select it again.
