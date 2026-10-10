# 02 — Graph files: open, watch, reload

How a graph file reaches the viewport, and what happens when it changes or breaks. Born in
P00 T3 (2026-10-10). The user-facing contract is in
`../command_reference/01-viewport.md` § Opening a graph.

## Pieces

| File | Role |
|---|---|
| `src/graph_file.{h,cpp}` | `loadGraphFile(path)` and `compileGraphFileText(text, path)` → `GraphLoad` {optional `GlslScene`, `error`, `pointer`}. They wrap `compileGraphText` (`01-render-path.md`): a `.json` name forces JSON, otherwise the text is sniffed. A `GraphError` becomes `field-graph error at <pointer>: <message>`, the CLI's wording. Any other `std::exception` from DualC is caught too. A fresh mesh resolver is made per load, so a reload re-reads meshes. `isGraphFileName` is the `.json`/`.fld` test the drop uses. |
| `src/main_window.{h,cpp}` | `MainWindow` (`QMainWindow`): the `Viewport` as central widget, `File ▸ Open`, `View ▸ Log`, the status bar and the Log dock. It owns the current file, its watch and the load reporting. |
| `src/viewport.{h,cpp}` | `Viewport::setScene(scene, frame)`: the only way a scene gets in. It emits `sceneApplied` or `sceneFailed(log)`. |
| `src/main.cpp` | The positional `graph` (default: the embedded `:/samples/gyroid_box.json`). The window calls `MainWindow::openFile`; `--snapshot` calls `loadGraphFile` and exits 2 on an error. |

## Swapping a scene (`Viewport::setScene`)

- **GL up.** `makeCurrent`, then `TraceRenderer::setScene`. It compiles and links the new
  program before it touches the old one, so on failure the old program, its mesh
  textures and the image on screen stay. Nothing repaints. `sceneFailed` carries the log.
  On success the scene is taken and `sceneApplied` is emitted. With `frame` (an open), or
  for the first scene, the camera is framed on the new bounds and the frame goes through
  the preview and settle path. Without it (a reload), the camera is kept, as in
  dualc_field_view, and one `update()` paints a full frame.
- **GL not up yet** (the argument is opened before `show()`). The scene is kept as
  pending, and `initializeGL` links it with the same signals. A later pending scene
  replaces an earlier one.
- **No GL 3.3 context.** `sceneFailed("no OpenGL 3.3 core context")`.
- On a context re-creation (reparenting), `initializeGL` relinks the scene on screen.

## Watching (`MainWindow`)

- `openFile` makes the path absolute (a `:/` resource stays as it is), watches it, and
  loads it with `frame`. A failed open still makes the file current and watched. That way
  a broken argument, or a broken drop, renders as soon as a save fixes it, while the last
  good scene stays on screen in the meantime.
- **Watch.** `QFileSystemWatcher` watches the file and its folder. An editor that saves by
  writing a new file and renaming it over the old one removes the watched inode. Qt then
  drops the file from the watch, and only the folder signal sees the new file.
  `fileEvent` re-adds the file when it exists again.
- **Debounce.** Every file or folder signal restarts a `kReloadDelayMs` = 100 ms
  single-shot. A save can come as several writes (truncate, then write), and this keeps a
  half-written file from being parsed. If the file is missing when the timer fires (in
  the middle of a rename), nothing happens and the folder signal comes again.
- **Only a real change.** `load` compares the bytes read with the last text tried
  (`text_`), and does nothing when they are equal. A touch, a save with no edit, or
  another file changing in the folder paints nothing, so the idle rule of P00 T2 holds.
  `text_` is set on failed attempts too, so the same broken text is not re-reported.
- **Not watched:** the mesh files a graph names. dualc_field_view does not watch them
  either.

## Mesh paths

`GraphDirMeshResolver` wraps DualC's `FileMeshResolver`. A relative `path=` is tried beside
the graph file first, then as given (the working directory, which is dualc_field_view's
only rule). A dropped graph finds the meshes that sit beside it, whatever directory the
editor was started from. The fallback keeps DualC's documented recipes working, such as
`mesh(path="data/foot.obj")` run from the DualC root. This is editor-side: DualC is not
changed (E-06).

## Error reporting

`reportError` writes the first line of the message to the status bar, prefixed with the
file name, and the whole message to the Log dock with a time stamp. Both get
`(keeping the previous scene)` when the viewport has a scene. Shader logs are several
lines long, so the status bar shows only their first line. `MainWindow::loadFinished(ok)`
marks the end of every load attempt, and the test waits on it.

## Verified (2026-10-10, P00 T3)

`main_window_test` drives a live `MainWindow` with synthetic drag and drop events, and
real file writes in a temporary folder. It passes on nouveau (NV92) and on llvmpipe. It
checks:

- a drop of `.json` and of `.fld` renders, and a `.txt` is refused;
- an in-place save and a write-and-rename save both reload, with the camera kept;
- a same-text save loads and paints nothing;
- a broken save puts its pointer (`/root/in/1`) in the status bar and the log, paints
  nothing, and the image stays pixel-identical;
- a fixed save renders the same image as before;
- a broken shader emits `sceneFailed`, and the image stays;
- the argument through `--snapshot`, with exit 2 for a broken or missing file;
- a relative mesh path beside the graph.

The File ▸ Open dialog is not driven by any test.
