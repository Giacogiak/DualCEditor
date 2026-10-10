# Handoff — P00 T3: open a graph file, by menu or by drag and drop

Date: 2026-10-10. Plan: `plan/10_P00_viewport.md` § T3. DualC pin: `2fcb6ee` (unchanged).

## What changed and why

- **`MainWindow`** (`src/main_window.{h,cpp}`, new). This replaces the bare `QMainWindow`
  in `main`. It holds:
  - the `Viewport`;
  - `File ▸ Open…` (`QKeySequence::Open`, filter `*.json *.fld`) and `File ▸ Quit`;
  - `View ▸ Log`, a bottom Log dock (read-only, five lines high), and the status bar.

  It accepts drops of local `.json`/`.fld` files (any case) anywhere on the window, the log
  included, and refuses other files at drag-enter. `openFile` frames the new graph and
  watches it. A failed open still makes the file current and watched, so the next save
  that fixes it renders it.
- **File watch.** `QFileSystemWatcher` watches the file and its folder. Any signal
  restarts a 100 ms single-shot (`kReloadDelayMs`). When it fires, the file is re-added if
  a rename-save dropped it, then re-read. The scene is recompiled only if the bytes differ
  from the last text tried. A reload keeps the camera, as dualc_field_view does. With no
  real change nothing is painted, so the T2 idle rule holds.
- **Errors.** A read error, a `GraphError` (with its JSON pointer) or a shader compile or
  link log goes to:
  - the status bar, first line only, with `(keeping the previous scene)` when a scene is
    on screen;
  - the Log dock, in full, with a time stamp.

  Nothing repaints, and the image stays.
- **`Viewport::setScene(scene, frame)`** with the `sceneApplied` and `sceneFailed(log)`
  signals. There is a new scene-less constructor, which shows the background and ignores
  `F`. Before GL is up, the scene is kept pending and linked in `initializeGL`. A failed
  link keeps the old program, textures and image: `TraceRenderer::setScene` already links
  before it touches the old program. The old `Viewport(scene, scale)` constructor is kept,
  so `viewport_test` is unchanged.
- **`src/graph_file.{h,cpp}`** (new). `loadGraphFile` and `compileGraphFileText` return a
  `GraphLoad`. A `.json` name forces JSON, otherwise the text is sniffed. Relative mesh
  paths resolve beside the graph file first, then in the working directory. This is a
  superset of dualc_field_view's rule (working directory only), so a dropped graph finds
  its meshes.
- **`main`.** A positional `[graph]` argument (at most one, a second exits 1), default
  `:/samples/gyroid_box.json`. `--snapshot` now renders that graph. A missing or broken
  file exits 2. In the window, a broken argument still opens the window, with the error
  shown.
- **Test** `main_window` (`tests/main_window_test.cpp`). It drives a live window with
  synthetic drag and drop events, and real writes in a temporary folder. It checks:
  - a drop of `.json` and of `.fld` renders, and a `.txt` is refused;
  - an in-place save and a write-and-rename save both re-render, with the camera kept
    after an orbit;
  - the file is still watched after the rename;
  - a same-text save loads and paints nothing;
  - a broken JSON shows `field-graph error at /root/in/1` in the status bar and the log,
    paints nothing, and leaves the image pixel-identical;
  - a fix renders the identical image;
  - a corrupted shader emits `sceneFailed`, and the scene and image stay;
  - `--snapshot <png> <file.fld>` works, and a broken or missing argument exits 2;
  - a relative `mesh(path="cube.obj")` beside the graph resolves, with the editor run from
    another directory.

  It skips (77) with no display, no GL 3.3 core context, or the `offscreen`/`minimal`
  platform.
- **Docs:**
  - `docs/command_reference/01-viewport.md`: the `graph` argument, snapshot exit 2, the
    ways to open a graph, reload on save, errors, the window's menus and dock.
  - `docs/design/02-graph-files.md` (new, indexed).
  - A pointer from `01-render-path.md`.
  - `docs/roadmap/README.md` current focus.

## Measured (development box, not the reference GPU)

Box: Quadro FX 3700 on nouveau (NV92), Mesa 26.0.8, Qt 6.10.2, Ubuntu, X11, Release build.
llvmpipe means `LIBGL_ALWAYS_SOFTWARE=1` (LLVM 21.1.8).

- From a save to the reload signal, in-place write, `main_window_test`: 150 ms on NV92 and
  110 ms on llvmpipe. That is the 100 ms debounce, plus parse and compile, plus the
  shader link.
- `main_window_test` takes about 32 s on NV92. Most of that is the 500 ms quiet waits and
  three `--snapshot` processes.
- Real binary, NV92, with `c.fld` as the argument and `--preview-scale 0.25`, untouched
  for 4 s: 3 preview frames, then 1 full frame, then nothing. With a broken
  `bad.json` as the argument, the window opens on the background (4 frames, then
  nothing) with the error shown.
- With the default 1200×800 window, the viewport is 1200×661 once the five-line Log dock,
  the menu bar and the status bar take their room. Before the dock was sized down, it was
  1200×539.

## What I got wrong on the way

- The first run of the test waited on `vp.windowHandle()` to be exposed. A non-native
  child widget has no window handle, so the wait timed out. It now waits on the top-level
  window's handle.
- The Log dock first opened at its default height, about 200 px, which left a 539 px high
  viewport in an 800 px window. It is now set to five lines with `resizeDocks`.

## Open

- **A human should check the real thing** (needsHumanCheck). Not covered by the tests:
  - a real drag from the file manager (the test sends synthetic `QDragEnterEvent` and
    `QDropEvent` straight to the window);
  - the `File ▸ Open` dialog, which no test drives;
  - saving from a real editor (vim, gedit, VS Code, Notepad++ on Windows), each with its own
    save strategy;
  - how readable the status-bar and log messages are, and the size of the Log dock.
- **Mesh files are not watched.** Editing a `.obj` a graph names does not reload it.
  dualc_field_view does not watch them either. If it is wanted, it is a small addition:
  watch the resolved paths.
- **Shorthand pointers** are DualC's best-effort character offsets, not JSON pointers. That
  is DualC's behaviour.
- **Only one graph is open at a time.** With several files in one drop, the first graph is
  opened and the rest are ignored silently. Multi-object scenes are P01.
- `main_window_test` on CI: it skips without a display, like `viewport_test`.

## For the next unit (T4)

- The scene on screen is `viewport()->scene()`. Every new scene comes in through
  `Viewport::setScene`, and `sceneApplied` is the signal on which to rebuild the
  parameter dock. A reload rebuilds the bindings, so the dock must follow it.
- To count link calls, put the counter in `TraceRenderer::setScene`. In T3, every load
  and every reload with changed text links exactly once. A failed link does not replace
  the program.
- The status bar already carries the load message (`Loaded <file>: N nodes, M
  parameters`). T4's compile and frame times need a place next to it, for example a
  permanent `QLabel` added with `addPermanentWidget`.
