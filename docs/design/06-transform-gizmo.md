# 06 — The transform gizmo

How the selected object is moved, rotated and scaled (P03 T3). Usage:
`../command_reference/01-viewport.md` § Transform.

## The pose

A pose is `SceneObject::transform` (`worldFromLocal`, rigid, as before) and
`SceneObject::scale`, a uniform factor about the object's origin, applied first. DualC's
`transform` node inverts its matrix with `Mat4::inverseRigid`, so a scale cannot live in the
matrix: it is DualC's `scale` node, `s * f(p / s)`, which keeps the field a distance. Only
a uniform scale keeps it one, so there is no per-axis scale.

- `lowerScene` wraps each object as `transform(scale(graph))`, the `scale` node always
  there, so a scale edit is a uniform (`03-scene-lowering.md` § Edits that do not compile).
- `operationGraph` writes the `scale` node only for a scale other than 1, under the
  `transform` node, which it writes only for a non-identity.
- The picker marches in the object's unscaled frame and scales the hit distance back.
- `SceneDocument::setPose(id, transform, scale, verb)` is one `SetPose` command, named
  `<verb> <name>`. A scale that is not positive is refused.
- The `.dcscene` writes `scale` beside `transform`; a file without it reads 1
  (`05-dcscene-format.md`).

## The arithmetic (`src/gizmo.{h,cpp}`, GL-free)

- `poseValues` / `poseFrom`: position, XYZ Euler degrees (R = Rz · Ry · Rx), scale. In
  gimbal lock the whole turn goes to z.
- `projectPoint` inverts `viewRay` (`pick.h`).
- `GizmoDrag(mode, start, camera, view, from).poseAt(to)`:
  - Move, free: the two cursor rays meet the plane through the origin facing the camera;
    the origin moves by the difference. On an axis: the nearest points of the two rays on
    the world axis through the origin.
  - Rotate: the angle the cursor sweeps around the origin's projection. A positive sweep
    on screen (counter-clockwise) turns positively about the axis toward the camera. A
    world axis pointing away from the camera flips the sign. The origin stays.
  - Scale: the start scale times the ratio of the cursor's screen distances to the origin.

## The window

- `Viewport` owns the modal drag. `setGizmoTarget(id, pose)` comes from the window at each
  selection or pose change. The keys `g`, `r`, `s` start a drag; `x`, `y`, `z` constrain it.
  `gizmoDragged` previews and `gizmoFinished(committed, mode)` ends it.
  `paintGizmo` draws the axes with `QPainter` over the traced frame.
- `MainWindow::previewPose` writes the pose into the scene on screen only and pushes its
  uniforms (`pushUniforms`): matrix, scale, culling box, trace box. The document is touched
  once, at the end, by `setPose`. A cancel re-syncs the object from the document.
- `TransformPanel` (the Transform dock) shows the document's pose of the selection. An edit
  emits the whole pose, keeping untyped components exact.

## Tested by

- `tests/gizmo_test.cpp` (ctest `gizmo`, no GL): pose values round-trip, gimbal lock,
  `projectPoint`, a 10 mm move along x, a free move under the cursor, a 90° turn about z, a
  doubled scale, `setPose` as one step with its undo, a non-positive scale refused,
  `sceneTraceBounds` equal to a new lowering's, the scale binding, and an operation over a
  moved, scaled operand.
- `tests/main_window_test.cpp`, gizmo block (GL): a gizmo drag along x and a typed x, each
  one step. The object is moved 3 box widths out of its trace box and framed with `F`, and it
  covers as many pixels as before (within 2%). The trace box follows the move and the
  scale, nothing compiles or links, a cancelled drag leaves the pose, and undo restores
  pose and box.
- `tests/scene_file_test.cpp`: an object saved with scale 1.5 reloads with it, with equal
  field values.
