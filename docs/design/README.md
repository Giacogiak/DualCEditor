# Design

How the editor is built today. Empty until code lands: each plan unit that settles a
structure (render path, scene model, `.dcscene` format, threading) writes its page here,
numbered in birth order, and lists it below.

## Files

- `01-render-path.md` — how a graph reaches the screen: GL calls through `QOpenGLFunctions_3_3_Core` (the choice and its cost), `TraceRenderer`, the uniform feed, the adaptive preview and settle, the `--snapshot` context.
- `02-graph-files.md` — how a graph file gets in: `loadGraphFile`, `MainWindow` (open, drop, the file watch with its debounce and rename handling, error reporting), `Viewport::setScene`, and mesh paths resolved beside the graph.
