# Design

How the editor is built today. Empty until code lands: each plan unit that settles a
structure (render path, scene model, `.dcscene` format, threading) writes its page here,
numbered in birth order, and lists it below.

## Files

- `01-render-path.md` — how a graph reaches the screen: GL calls through `QOpenGLFunctions_3_3_Core` (the choice and its cost), `TraceRenderer`, the uniform feed, the `--snapshot` context.
