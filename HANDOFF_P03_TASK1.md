# Handoff — P03 T1: operation history and undo

Date: 2026-10-13. Plan: `plan/40_P03_scene_model.md` § T1. DualC pin: `2fcb6ee` (unchanged).

**Status: done.** Every scene edit goes through a command on a `QUndoStack`. The model is
in `docs/design/04-scene-model.md`, and E-25 is closed. `python scripts/check.py` exits 0
on the dev box (NV92), with 14/14 ctests. A GTX check is owed (§ GTX check owed).

## What changed and why

- **`src/scene_document.{h,cpp}` (new): `SceneDocument`.** It holds the `Scene` and a
  `QUndoStack`. There is one command per edit:
  - `AddObject` and `RemoveObject` keep the object, its id and its index;
  - `SetField<name|transform|visible|colour>`;
  - `SetParameter` merges consecutive edits of one parameter, and an edit dragged back to
    its start leaves no step;
  - `ReplaceGraph` is a file reload;
  - `ApplyOperation` adds the result and hides the operands that were shown;
  - `beginMacro`/`endMacro` make a reload of several files one step;
  - `reset` is File ▸ Open: a new document with no history.

  The document sends one `changed(Changes)` (params, objects, structure) at the end of
  each push, undo, redo, merge or macro. It does no GL.
- **`operationGraph`** builds the operation's graph: the op node over each operand's graph,
  each wrapped in `transform` when the operand is moved.
  - Every node gets a structural pointer, because operands can share `/root` or shorthand
    offsets.
  - The operands' edits are re-keyed to those pointers.
  - When the operands' `meshDir`s differ, relative mesh paths become absolute.

  There is no UI for operations yet: P05 builds the panels.
- **`SceneObject` gained `file`, `edits` and `operands`** (`src/scene.h`).
  - `edits` holds parameter edits as uniform values keyed by `(nodePointer, paramKey)`.
    It is not written into the graph, because a uniform is not always a param (`angleRad`,
    defaults, the `mat4` of `translate`/`rotate`).
  - `lowerScene` applies the edits (`applyEdits`).
  - `Scene` gained `insert` (keeps the id), `indexOf` and `nextId`.
- **`MainWindow` edits only through the document.**
  - `Doc::files` is gone: each object carries its `file`.
  - `documentChanged` sets the uniforms of the scene on screen at once (`syncObject`:
    visible, colour, matrix, culling box) or, for a structural change, calls `apply`. How a
    load applies is set in `next_` by the caller.
  - The Parameters dock's edits of `matrix`, `visible` and `colour` become
    `setTransform`/`setVisible`/`setColour`. Before this, a `matrix` edit never reached
    `SceneObject::transform`; now it does.
  - When a scene cannot be shown, `revertDocument` undoes or redoes back to `shownIndex_`.
    After a failed open it resets the document to the scene on screen.
- **UI:**
  - `Edit ▸ Undo`/`Redo` (`QKeySequence::Undo`/`Redo`);
  - `Edit ▸ Rename Object` (`F2`);
  - Objects rows are editable (`ObjectsPanel::renameRequested`).

  The command reference has a new § Undo and redo.
- **Tests:**
  - `tests/scene_history_test.cpp` (new ctest `scene_history`, GL-free) runs 11 commands:
    3 adds, rename, move, hide, recolour, a merged parameter edit, union, reload, remove.
    It undoes them to the empty scene and redoes them to the end. At every step it checks
    `sameScene` (every SceneObject field, graphs node for node) and that `lowerScene`
    gives the same source and the same uniform values. It also checks merging, the
    obsolete edit, that ids are not reused, macros and reset.
  - `tests/main_window_test.cpp` gains a block: hide all, undo restores the image, with no
    link; redo; rename reaches the row; remove, then undo, gives the same source and
    uniform values.
- **Plan corrected:** the design page is `04-scene-model.md`, not `03-…`, which was taken by
  `03-scene-lowering.md`. T2's format page becomes `05-dcscene-format.md`.

## Measured

There are no performance numbers. This unit adds no GPU work. A hide, an undo and a redo
link nothing (`vp.linkCount()` is unchanged, main_window test, NV92). Gate: 14/14 ctests,
dev box, Release.

## What I got wrong on the way

- A merged parameter edit that became obsolete left an edit entry equal to the default:
  `QUndoStack` drops an obsolete command without undoing it. `mergeWith` now undoes it
  itself. `scene_history` caught this.
- At first `sameScene` compared `nextId`. An undone add keeps the counter advanced by
  design, so the scene is compared object by object only.
- The window test's first hide picked the front object of two, and the image did not
  change, because the other object covers it. It now hides every object.
- "Undo of a remove restores the image" failed on NV92. The source and every uniform value
  were identical, but 134 268 px differed. The pre-remove frame was drawn oddly (a cyan
  background). The check now compares the source and the uniform values. Whether the
  image matches across the relink is the GTX check below.

## Open

- **An operation's result is a copy** of its operands' graphs (E-24, DAG references, stays
  deferred). Editing an operand afterwards does not change the result.
- **Parameter edits do not reach the graph file.** They live in `SceneObject::edits`. T2's
  `.dcscene` must save `edits`, `operands` and `file`, the last as a relative name.
- **The NV92 image difference after a relink.** It is above, unexplained, and it may be
  the dev box's driver.
- A reload is now an undo step. Undoing it shows the old graph while the file on disk has
  the new text. The file is not re-read until it changes again.

## For the next unit (P03 T2)

- Save `SceneDocument::scene()`. Load with `SceneDocument::reset` and, in the window, with
  `next_ = {frame, loaded, attempt, …, opened = true}`, as `MainWindow::openFile` does.
- `sameScene` and the `lowered()` fingerprint in `scene_history_test.cpp` are ready-made
  round-trip comparisons.
- Meshes: `operationGraph` may write absolute mesh paths when operands come from different
  folders. T2 needs relative names (and P04 in-memory ids), so revisit this there.
- P03 T3 (gizmo): call `SceneDocument::setTransform`. The window already pushes the matrix
  and the culling box as uniforms (`syncObject`). For one step per drag, use a macro, or a
  merge id on `SetTransform`.

## GTX check owed

On the GTX 1050 Ti laptop, a Release build of this commit:
1. `ctest --test-dir build --output-on-failure`. Expected: 14 tests, with `scene_history`
   passing. `main_window` keeps its known failure at the ".fld renders a different image"
   check; any other failure in its new undo block is news.
2. `DualCEditor a.fld b.fld` (any two graphs, e.g. DualC's samples), then:
   - Uncheck an object, then `Ctrl+Z`: it shows again at once, and the status bar's
     compile ms does not change.
   - Delete an object, then `Ctrl+Z`: it comes back with its colour and name, and the image
     is the same as before the delete. **This is the image question NV92 could not
     answer.**
   - `F2` on a row, type a name, `Enter`: the row and the Parameters section are renamed.
     `Ctrl+Z` brings the old name back.
   - Step a parameter spin box a few times, then `Ctrl+Z` once: the value before the first
     step comes back.
   - With AZERTY: `Ctrl+Z` undoes, and `Ctrl+Y` (Windows) redoes.
