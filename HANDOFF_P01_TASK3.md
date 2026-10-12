# Handoff — P01 T3: Click to select

Date: 2026-10-12. Plan: `plan/20_P01_scene_and_picking.md` § T3. DualC pin: `2fcb6ee` (unchanged).

**Status: done** on the automatic checks. A human look is asked for (below): the highlight
colours and the click-versus-drag feel were judged by tests on this box, not by eye.

## What changed and why

- **`src/pick.{h,cpp}`** (new), the CPU pick of E-07. `viewRay` is the trace shader's
  `rd` for a widget point. `ScenePicker(scene, lowered)` builds each object's own graph
  with `FieldGraph::build` (local frame, own `GraphDirMeshResolver`). `pick(ray)` takes
  the ray into each visible object's frame through `localFromWorld`, clips it to the
  object's local trace box and marches it as the shader does: gradient-normalised steps
  for a non-metric object, `TraceRenderer`'s step bounds from the object's box and
  `featureScale`, then 20 bisections. The smallest `t` wins. `setVisible` and
  `setLocalFromWorld` follow uniform edits.
- **`LoweredObject`** gains `stepScale` and `featureScale` (DualC's, per object), so the
  pick marches each object with its own constants.
- **Highlight.** `sceneColor` now records the id of the object that coloured the hit and
  reads one scene uniform `u_selected` (`kSelectedUniform`, `kSelectionTint` in
  `scene.h`). The selected object is mixed 0.45 toward amber (1.0, 0.78, 0.20), and the
  others are dimmed to 0.45 while something is selected. **It is not a binding**:
  `TraceRenderer::setSelectedObject` stores it, and `render` uploads it every frame. So
  the Parameters dock does not show it, `MainWindow::apply`'s value carry-over does not
  touch it, and it survives a relink. A single graph's shader has no `u_selected`, so the
  upload there does nothing.
- **`Viewport`**: `clicked(QPointF)` on a plain left press and release that did not move
  past `QApplication::startDragDistance()`. `setSelectedObject(id)` repaints once at full
  resolution. **Behaviour change**: an orbit or pan now starts only past the drag
  distance, and the whole movement then applies, so a click does not turn the view.
- **`MainWindow::pickAt(pos)`** (public): builds the picker lazily at the first click after
  a commit (`commit` drops it), syncs visibility from the `Scene` and transforms from the
  viewport's binding values, picks, then calls `selectObject`. `selectObject` now also
  calls `viewport_->setSelectedObject`, so the viewport, the dock and `selectionChanged`
  agree in both directions.
- **Tests** (12 ctests): new `pick` (GL-free). `scene` checks the `sceneColor` id and
  `u_selected` text. `objects_panel` gains a click section (below). No passing test was
  edited to pass.
- **Docs**: design 03 § Picking and the selection highlight, and the `sceneColor` listing;
  command reference § Objects (click, highlight) and § Mouse and keys (click, drag
  distance); roadmap current focus; plan T3 notes.

## Measured

`pick_test` (CPU, any box): two spheres of radius 0.5, at the origin and at
(0.3, 0, −2), camera at z = 5. The centre ray picks the front one at **t = 4.500000**
(exact 4.5). With it hidden, the back one is picked at 7 − √0.16 to within 1e-4. A
gyroid-in-box (non-metric) is hit at t = 4.208384 with **field −3.3e-9**. Nothing on the
ray before the hit is inside, checked every 1e-3.

`objects_panel_test` on this box (Quadro FX 3700, nouveau NV92, Mesa 26.0.8, X11, 800×500,
preview off), with the three side-by-side graphs: a click at the centroid of the orange
pixels selects `b` in the window and the dock, changes the image, and leaves yaw and pitch
unchanged. A click at (4, 4) clears the selection and the image equals the unselected one
pixel for pixel. **Links 3 → 3, compiles 6 → 6** across every selection. Selecting a row
in the dock also changes the viewport image.

`python scripts/check.py`: green, 12/12 ctests pass here, none skipped.

## What I got wrong on the way

- The new test included `pick.h` and `graph_scene.h` but used `GraphDirMeshResolver` from
  `graph_file.h`. It failed to build; I added the include.

## Open

- **Parameter edits other than `matrix` and `visible` are not seen by the pick.** The C++
  field is built from the file's values. Writing a uniform back into the graph needs
  per-op knowledge of DualC's flat `params` packing. Options: a DualC helper that applies
  a binding to a `GraphNode` (a DualC PR, rule 5), or editor-side per-op tables. This
  matters once parameter edits get large enough to move a surface by more than a click's
  worth.
- **The picker builds on the first click after each commit.** For a mesh object that is a
  BVH build, which may take a noticeable time on a large mesh. It was not measured: there
  is no mesh scene in the tests. Building it after commit, in the background, would hide
  that cost.
- **Highlight look.** The amber tint and the 0.45 dim are first choices. For the yellow
  palette object the tint is weak, and the others' dimming carries the signal. No outline,
  because `sceneColor` has no view direction. To be judged on the GTX
  (`render-tuning-deferred` rule).
- Shift+click is left free (a future add-to-selection key). Selection is single.
- The pick uses its own step bounds per object, not the scene-wide ones the shader uses,
  so it can be finer than the image. On thin TPMS walls it may select an object whose
  wall the preview stepped over. That is the exact field's answer.

## For the next unit (T4, frame time against object count, a human check)

- The `u_selected` upload is one `glUniform1f` per frame, plus one `if` and one compare
  per pixel in `sceneColor`. It should not show in T4's numbers. Measure with nothing
  selected (the `--bench` path never selects).
- `--bench` and `--snapshot` with several graphs are unchanged.

## Human check asked

On the laptop: `./build/bin/DualCEditor a.fld b.fld c.fld` with the three files of
`docs/command_reference/01-viewport.md` § Several graphs.
- Click each object. It is tinted amber, the others dim, and its row is selected in the
  Objects dock.
- Click the background. Every colour comes back, and no row is selected.
- Select rows in the dock. The viewport highlight follows.
- Orbit until two objects overlap on screen, then click the overlap. The front one is
  selected.
- A normal click does not nudge the view. A short drag still orbits without feeling
  sticky (the drag starts after the platform's drag distance).
- Hide an object and click where it was. The object behind it, or nothing, is selected.
