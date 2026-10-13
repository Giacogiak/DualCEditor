# Design

How the editor is built today. Empty until code lands: each plan unit that settles a
structure (render path, scene model, `.dcscene` format, threading) writes its page here,
numbered in birth order, and lists it below.

## Files

- `01-render-path.md` — how a graph reaches the screen: GL calls through `QOpenGLFunctions_3_3_Core` (the choice and its cost), `TraceRenderer`, the uniform feed, the adaptive preview and settle, parameter edits (uniform only, link count) and the compile and GPU frame timing, the `--snapshot` context.
- `02-graph-files.md` — how a graph file gets in: `loadGraphFile`, `MainWindow` (open, drop, the file watch with its debounce and rename handling, error reporting), `Viewport::setScene`, and mesh paths resolved beside the graph.
- `03-scene-lowering.md` — how a scene of several objects becomes one shader: `Scene` and `SceneObject`, `lowerScene` (each object wrapped in `transform`, compiled on its own and renamed with its id), the union and `sceneColor`, the colour patch of DualC's framework, visibility and colour as uniforms, and why it is not one DualC graph.
- `04-scene-model.md` — how the scene is edited and undone: `SceneDocument` and its `QUndoStack`, one command per edit (add, remove, rename, move, show, recolour, parameter edit, reload, operation), parameter edits kept beside the graph, the `changed` signal the window applies, the revert when a scene cannot be shown, and operations as copies over hidden operands.
- `06-transform-gizmo.md` — the pose (rigid transform and uniform scale, DualC's `scale` node under `transform`), the gizmo's arithmetic (`src/gizmo`), the modal drag, the Transform dock, and the trace box following a move as uniforms.
- `05-dcscene-format.md` — the scene file: a zip of `scene.json` and the meshes' bytes (E-08, miniz through DualC), the graph written with its node pointers, no absolute path (files relative to the scene, meshes named by entry), the version check, extraction on load, and the window's open and save.
