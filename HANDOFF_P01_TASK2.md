# Handoff — P01 T2: Objects panel

Date: 2026-10-12. Plan: `plan/20_P01_scene_and_picking.md` § T2. DualC pin: `2fcb6ee` (unchanged).

**Status: done** on the automatic checks. A human look is asked for (below): the dock's
layout, the swatches and the Delete key were driven by a test on this box, not used by hand.

## What changed and why

- **`src/objects_panel.{h,cpp}`** (new). `ObjectsPanel`, a `QListWidget`: one row per
  object (checkbox, 16 px colour swatch with the shader's 1/2.2 gamma, name; the id in
  the tooltip), single selection, `Delete` matched by meaning (`QKeySequence::Delete`).
  It only emits `visibilityToggled`, `selectionMoved`, `deleteRequested`.
- **`MainWindow` holds a `Scene`** (`Doc`: scene + object id → file). Every structural
  change (open, add, delete, a reloaded file) lowers the scene and calls
  `Viewport::setScene`; the new doc is committed on `sceneApplied`, so a failed lowering
  or link leaves the scene on screen and the doc unchanged. Show/hide sets
  `u_o<id>_visible` through `Viewport::setParameter`, and syncs the Parameters dock
  (`ParameterPanel::showValue`, no signal).
  - `openFile` replaces the scene with one object (File ▸ Open, first argument);
    `addFile` adds one (drop, File ▸ Add…, further arguments). A drop adds every graph
    file it carries. `removeObject`, `setObjectVisible`, `selectObject`,
    `selectedObject`, `selectionChanged`, `scene()`, `lowered()`, `objects()` are public.
  - **Edits survive re-lowering.** Bindings of objects whose graph did not change keep
    their current values (matched by the object's binding range and `paramKey`); a
    reloaded file's objects take the file's values. Visible and colour come from the
    `Scene`, which follows the Objects dock and edits of `visible`/`colour` in the
    Parameters dock.
  - **Watching** covers every object's file plus "waiting" files (an open or add that
    failed: the save that fixes it opens or adds it, as the single-file window did).
    Files that change together reload as one attempt.
- **`ParameterPanel`**: optional `sections` (a title row with the object's name above its
  first binding) and `showValue`. The window prefixes each graph binding's pointer with
  `/objects/<id>` for display, since two graphs share pointers (`/root/...`).
- **`Viewport::clearScene` / `TraceRenderer::clearScene`**: deleting the last object
  shows the background, links nothing.
- **`main.cpp`**: the window takes several `graph` arguments instead of exiting 1.
- **`graph_file`**: `parseGraphFileText` (parse text already read, for the text dedup).
  **`Scene::find`** has a const overload. `scene.h`'s comment pointed at the old page
  number `02-scene-lowering.md`; fixed to `03`.
- **Tests** (11 ctests): new `objects_panel` (GL, skips 77). `main_window` adapted (below).
- **Docs**: command reference § *Objects* (new), *Opening a graph*, *Reload on save*,
  *Errors*, *Window*, *Parameters*, the `graph` row and recipes; design 03 § Where it is
  used; roadmap current focus; plan T2 notes.

## Measured (development box, not the reference GPU)

Quadro FX 3700, nouveau NV92, Mesa 26.0.8, X11, `objects_panel_test`, window 800×500,
preview off; the three side-by-side graphs of the command reference (grey sphere, orange
box, blue sphere):
- two drops (`a.fld`, then `b.fld`+`c.fld` in one drag) → 3 rows, 3 objects; the image
  has **7009 orange and 7292 blue px of 200 716**;
- hide `b` from its checkbox then show it: **links 3 → 3, shader compiles 6 → 6**, no
  orange left while hidden, and the shown image equals the earlier one pixel for pixel;
- Delete on the selected row: one object fewer, its colour gone, selection cleared; the
  last Delete leaves `hasScene() == false` and the background.

`python scripts/check.py`: green, 11/11 ctests pass here (none skipped).

## What I got wrong on the way

- `main_window_test` failed at "the reload re-renders": under the new rule its second
  drop *adds* the cube to the gyroid, which encloses and hides it, so editing the cube
  changed no pixel. The test encoded the old "a drop replaces"; the plan changes that
  behaviour. I added, after the second drop, a check that there are two objects and a
  `removeObject` of the gyroid; nothing else in the test changed. The plan's T2 notes say so.

## Open

- **Drop onto the default sample.** With no `graph` argument the window opens on
  `gyroid_box.json`, so the first drop adds to it; the user deletes it from the dock.
  Replacing an untouched default on the first drop would be friendlier; not done
  (it is a rule the plan does not state).
- **No colour picker.** The swatch is display only; the colour is edited in the
  Parameters dock (`colour`), and the swatch follows.
- **After File ▸ Open the ids start again at 1**: if object 1 was selected, the new
  object 1 is selected.
- **A shader link failure of an added file** is reported, but the file does not wait for
  a fix (a lowering or parse failure does). DualC's codegen should not emit unlinkable
  code, so this was left.
- The `matrix` edit from the Parameters dock survives re-lowering, but the trace box is
  still the lowered one (T1 open point).

## For the next unit (T3, click to select)

- Selection lives in `MainWindow::selected_`; `selectObject(id)` updates the dock and
  emits `selectionChanged`. The viewport pick should call `selectObject` (0 for empty
  space), and the highlight should follow `selectionChanged`, both directions for free.
- `lowered().objects[i]` gives each object's `worldBounds` and binding indices; the
  current `localFromWorld` values (edits included) are in
  `viewport()->scene().bindings[transformBinding]`.
- The Viewport has no idea of objects; a highlight uniform would be one more per-object
  uniform in `lowerScene` (e.g. `u_o<id>_selected`), set like `visible`.

## Human check asked

On the laptop: `./build/bin/DualCEditor a.fld b.fld c.fld` with the three files of
`docs/command_reference/01-viewport.md` § Several graphs. Check the Objects dock sits above
Parameters at a usable height; the swatches read as grey, orange and blue; unchecking a row
hides that object at once; clicking rows selects; `Delete` (Belgian AZERTY "Suppr") with the
dock focused removes the selected object; and dropping another `.fld` from the file manager
adds a fourth row and object.
