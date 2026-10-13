# Handoff — P03 T3: transform gizmo

Date: 2026-10-13. Plan: `plan/40_P03_scene_model.md` § T3. DualC pin: `2fcb6ee` (unchanged).

**Status: done, and P03 with it.** You can move, rotate and scale the selected object from
the viewport (keys `G`, `R`, `S`, with `X`/`Y`/`Z` for an axis) and from a new Transform
dock that takes millimetres and degrees. None of this compiles anything: the trace box and
the culling box follow the move as uniforms. The design is in
`docs/design/06-transform-gizmo.md` and the usage in `docs/command_reference/01-viewport.md`
§ Transform. `python scripts/check.py` exits 0 on the dev box (NV92), with 16/16 ctests.
A GTX check is owed (§ GTX check owed).

## What changed and why

- **Scale is a uniform factor, `SceneObject::scale`, beside the rigid `transform`.**
  DualC's `transform` node inverts with `inverseRigid`, so a scale cannot go in the matrix.
  DualC's `scale` node (`s * f(p/s)`) keeps the field a distance, but only for a uniform
  scale. So there is no per-axis scale. The plan says "scale" without settling this, and I
  took the option that keeps the field exact.
  - `lowerScene` wraps every object as `transform(scale(graph))`. The scale node is always
    present, so a scale edit is the uniform `s` (`LoweredObject::scaleBinding`). The two
    wrapper nodes share the pointer `/objects/<id>`.
  - `operationGraph` writes `scale` only when it is not 1.
  - The picker divides by the scale (`ScenePicker::setScale`).
  - `worldBox(local, transform, scale)` gives the boxes.
  - `.dcscene` gains `objects[].scale`. It is optional and reads 1 when absent; a value
    that is not positive is refused. The version stays 1, since no older file can be
    misread.
- **`SceneDocument::setPose`** is one `SetPose` command for the transform and the scale,
  named `Move|Rotate|Scale|Transform <name>`.
- **Trace box follows** (`docs/roadmap/03-go-no-go.md` item 2):
  - `sceneTraceBounds(scene, lowered)` (in `scene.h`) is the union of every object's
    box, posed as it is now;
  - `TraceRenderer::setBounds` and `Viewport::setTraceBounds` set it as
    `uBoundsMin`/`uBoundsMax`;
  - `MainWindow::pushUniforms`, split out of `syncObject`, pushes the matrix, the scale,
    the culling box and the trace box.
- **Gizmo:**
  - `src/gizmo.{h,cpp}` is GL-free and holds `Pose`, `PoseValues` (XYZ Euler degrees),
    `projectPoint` and `GizmoDrag`.
  - `Viewport` runs the modal drag, draws the axes with `QPainter` after the frame, and
    emits `gizmoDragged` and `gizmoFinished`.
  - The window previews a drag into the scene on screen only, and pushes the pose to the
    document once, when the drag ends.
- **Transform dock** (`src/transform_panel.{h,cpp}`): position in mm, rotation in degrees,
  and the scale. The boxes do not track each keystroke, so a typed value is one undo step,
  and a component you did not type keeps its exact value. It is listed in the View menu.
- **Tests:**
  - New ctest `gizmo`, GL-free.
  - `main_window_test` gains a gizmo block.
  - `scene_file_test`: the ball is saved with scale 1.5 (`setPose`). The `samples` and
    `sameObjects` helpers now include the scale; nothing they checked before was relaxed.
- **Docs:**
  - new `06-transform-gizmo.md`, listed in the design README;
  - `03-scene-lowering.md` (§ Edits that do not compile);
  - `04-scene-model.md` (the SetPose row);
  - `05-dcscene-format.md` (the `scale` key);
  - the command reference (§ Transform, Window table, View menu, Mouse and keys);
  - the roadmap focus.

## Acceptance and exit criteria

| Criterion | Evidence |
|---|---|
| T3: a transformed object renders, saves and reloads at the same pose | `scene_file`: the gyroid is moved and rotated, the ball moved and scaled 1.5. After save and load, `sameObjects` matches (matrix memcmp, scale `==`) and the 343 field samples per object are equal. `main_window` gizmo block: the moved object renders. |
| T3: a moved object is still drawn whole, with no compile; the trace box and culling box follow as uniforms | `main_window` (NV92, Release): the dock moves the gyroid 3 box widths, so it lies wholly outside its old trace box, and `F` frames it. It then covers as many pixels as before the move, within 2%. Before T3, F framed the old box and the object covered 0 pixels. `vp.scene().bounds.min.x > old max.x`. `linkCount` and `compileCount` are unchanged across a drag, a typed move and a scale. `gizmo`: `sceneTraceBounds` after a move equals a fresh `lowerScene`'s bounds, and the culling box is `worldBox(local, transform, scale)`. |
| T3: the command-reference page is updated | `01-viewport.md` § Transform (Gizmo, Transform dock), the Window table, and Mouse and keys. |
| T1: undo and redo restore the exact scene; GL-free round-trip ctest | `scene_history` passes, unchanged (16/16 run). `gizmo` adds the undo and redo of a pose. |
| T2: save then load reproduces the scene; a future version is refused; a moved file loads | `scene_file` passes, now with a scale in the scene. |
| Exit: T1 to T3 met, `python scripts/check.py` green | `python scripts/check.py` exit 0 on the dev box, 16/16 ctests. |

The plan's criteria were right as written. The plan file is unchanged.

## Measured

No performance numbers: the dev box cannot measure them (E-04). Two costs are added to
every lowered object: a `scale` wrapper function (one divide and one multiply per
evaluation) and a `float` uniform. The GTX check below times them against P07's numbers.

## What I got wrong on the way

- **The window test's step counts.** I first used `QUndoStack::count()`, which includes
  steps that have been undone. The block hides and un-hides the object to count its
  pixels, so `count()` was off. I switched to `index()`.
- **A typed x that the box rounds.** The first test typed 3 box widths, a value with more
  than 3 decimals, and the box rounded it. It now types whole millimetres.
- **The sphere parameter is `radius`, not `r`.** I took the wrong name in the GL-free test's
  fixture and corrected it.

## Open

- **`featureScale` does not follow a scale.** The march's step bounds come from the trace
  box (which follows the pose) and from `featureScale` (which stays as lowered). A
  scale-down by a large factor without a recompile could therefore step past thin
  features until the next structural change. Watch for it on the GTX (step 3 below). If it
  shows, the fix is to scale `featureScale` with the smallest object scale in
  `pushUniforms`.
- **No snapping** (by mm steps or angles) during a drag; the dock gives exact values.
- **The gizmo has no handles.** It is key-driven (Blender style): axes are drawn, but there
  is nothing to drag directly.
- **The overlay draws with `QPainter` over the GL frame.** It is not counted in GPU frame
  timing (it is drawn after the timer query ends).
- A rotation typed in the dock goes through Euler angles: a pose made by the gizmo shows its
  decomposition, which near gimbal lock (y = ±90°) puts the whole turn in z.

## For the next unit (P04)

- Every object now lowers as `transform(scale(graph))`. A mesh import (P04) gets a pose,
  scale included, for free.
- `SceneObject::scale` is part of `sameScene`, the `.dcscene` and `operationGraph`.

## GTX check owed

On the GTX 1050 Ti laptop, a Release build of this commit:
1. `ctest --test-dir build --output-on-failure`. Expected: 16 tests. `gizmo` passes.
   `main_window` keeps only its known failure (".fld renders a different image"); any
   failure in the gizmo block is news. In particular, "the moved object is drawn whole"
   checks pixel counts on this GPU.
2. `DualCEditor <gyroid_box.json> <any second graph>`, on Belgian AZERTY:
   - Click an object. Red, green and blue axes appear at its origin, and the Transform dock
     shows its name and pose.
   - `G`, then move the mouse: the object follows the cursor. `X`: it slides along the
     red axis only, which is drawn across the view, and the bottom-left reads
     `Move x: … mm`. Click: one `Move <name>` step in Edit.
   - `R`, then `Z`, then sweep around the object: it turns about z. `Esc`: it goes back.
   - `S`, then move away from the object: it grows; click.
   - Type 200 in Position x and press Return. The object jumps 200 mm, still whole: no
     clipping at the old box, and no compile line in the log.
   - `Ctrl+Z` three times: back where it was. `Ctrl+S` still saves; the dialog opens and no
     scale starts.
   - **Frame time while dragging** (status bar): about P07's numbers for this scene, with
     no new stalls during a drag.
3. Scale the gyroid to 0.2 in the dock and orbit. Look for missing or noisy thin walls
   compared to the same scene saved, closed and reopened (which recompiles). A difference
   means `featureScale` must follow the scale (§ Open).
4. Save the scene with a scaled, rotated object as a `.dcscene`, reopen it: the same pose.
