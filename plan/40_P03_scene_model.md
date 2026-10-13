# P03 — Scene model, undo, `.dcscene`

Waits on **E-08** (scene file format). From here on, an operation produces a new object
whose field graph refers to its operands. The scene keeps that history, and saving it
keeps the history too.

### T1. Operation history and undo

Every change goes through a command on a `QUndoStack`. Commands cover:
- adding, removing and renaming an object;
- transforms;
- visibility;
- parameter edits;
- operations.

An operation hides its operands, but does not delete them. Record the model in
`docs/design/03-scene-model.md`, and close E-25 there.

#### Acceptance criteria
- Undo and redo restore the exact scene: the same graphs and the same visibility.
- A ctest does a round trip through a sequence of commands without GL.

### T2. Save and load `.dcscene`

Implement the format chosen in E-08:
- a `version` field that the reader checks, rejecting unknown versions with a clear message;
- embedded meshes;
- relative names, never absolute paths.

Document the format in `docs/design/04-dcscene-format.md`.

#### Acceptance criteria
- A save then load reproduces the scene. The ctest compares field values at sample points,
  and the mesh buffers byte for byte.
- A file with a future `version` is refused with a message.
- Moving the file to another directory still loads it.

### T3. Transform gizmo

Move, rotate and scale the selected object from the viewport and from a numeric panel, in
millimetres. Values are typed in, the same way DualC's graph `units` are.

#### Acceptance criteria
- A transformed object renders, saves and reloads at the same pose.
- A moved object is still drawn whole, with no compile: the trace box (`uBoundsMin`,
  `uBoundsMax`) and the object's culling box (P07 T1) follow the move as uniforms
  (`docs/roadmap/03-go-no-go.md`, item 2).
- The command-reference page is updated.

## Exit criteria
- Tasks T1 to T3 are met. `python scripts/check.py` is green.
