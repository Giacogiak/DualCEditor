# 01 — Viewport, files and launch flags

`DualCEditor [options] [graph]`

`graph` is a field graph: a `.json` document, or a `.fld` file in DualC's text shorthand
(`difference(box(min=[-1,-1,-1],max=[1,1,1]),sphere(radius=1.25))`). A name ending in
`.json` is read as JSON. Any other name is sniffed: a leading `{` is JSON, anything else is
shorthand, as in `dualc_field_view`. With no `graph`, the window opens on DualC's
`examples/samples/gyroid_box.json`, which is embedded in the binary from the pinned DualC
tree. A graph opens on the framed default view of `dualc_field_view`:
yaw 0.7, pitch 0.5, distance 1.3 × the diagonal of the trace box. The trace box is the
field's own bounds padded by 5%.

## Flags

| Flag | Default | Effect |
|---|---|---|
| `graph` (positional) | `gyroid_box.json` | The graph to open. At most one; a second exits 1. In the window, a graph that does not load still opens the window, with the error in the status bar and the log, and is watched, so fixing it renders it. |
| `--snapshot PATH` | off | Render one 800×600 frame of `graph` (default view) off-screen to the PNG `PATH`, then exit. Opens no window. |
| `--preview-scale S` | `0.5` | Resolution factor while the view moves. The view is traced at `S` × the window's pixels, then upscaled linearly. It returns to full resolution 0.18 s after the last input. Clamped to 0.1 – 1. `1` turns the preview off. A value that is not a number exits 1. Same flag and default as `dualc_field_view`. |
| `-h`, `--help` | | Print usage and exit. |
| `-v`, `--version` | | Print the version and exit. |

Exit codes of `--snapshot`:

| Code | Meaning |
|---|---|
| 0 | PNG written. |
| 1 | Shader link or PNG write failed (the reason goes to stderr). |
| 2 | The graph cannot be read (`cannot read <path>: …`), or does not parse or compile (`field-graph error at <pointer>: …`). |
| 77 | No OpenGL 3.3 core context. The ctest reports this as skipped. |

On Linux with no `DISPLAY` and no `WAYLAND_DISPLAY`, `--snapshot` selects the `offscreen`
Qt platform unless `QT_QPA_PLATFORM` is set. With no X display that platform has no GL,
so the result is exit 77.

## Recipes

```bash
./build/bin/DualCEditor                                   # open the viewport
./build/bin/DualCEditor part.fld                          # open (and watch) a graph
./build/bin/DualCEditor --preview-scale 0.25              # coarser, faster preview while moving
./build/bin/DualCEditor --snapshot out.png                # one frame, hardware GL
./build/bin/DualCEditor --snapshot out.png part.json      # one frame of part.json
LIBGL_ALWAYS_SOFTWARE=1 ./build/bin/DualCEditor --snapshot out.png   # Mesa llvmpipe
xvfb-run -s "-screen 0 1024x768x24" ./build/bin/DualCEditor --snapshot out.png  # no display (not yet run: xvfb is not installed on the dev box)
```

## Opening a graph

| Way | Behaviour |
|---|---|
| `File ▸ Open…` (`Ctrl+O`, the platform's Open shortcut) | File dialog filtered on `*.json *.fld`, with `All files` as the other filter. Starts in the open file's folder. |
| The `graph` argument | As above. |
| Drop a file on the window | Accepted when the drag carries a local file ending in `.json` or `.fld` (any case). Other files are refused: the cursor shows no drop. With several files, the first graph is opened. A drop anywhere on the window counts, the viewport and the log included. |

Opening frames the camera on the new graph's trace box, like `F`. The window title becomes
`<file name> - DualCEditor`.

A relative mesh path (`mesh(path="cube.obj")`) is looked up beside the graph file first,
then in the working directory. `dualc_field_view` looks only in the working directory.

### Reload on save

The open file is watched (`QFileSystemWatcher`, the file and its folder). When it changes
on disk, it is re-read 0.1 s after the last change signal, and re-rendered only if its text
differs from the text last loaded. The camera is kept, as in `dualc_field_view`. Press `F`
to frame the new bounds. A save that renames a new file over the old one (as many editors
do) is followed too. A save with the same text, or a change to another file in the folder,
loads and paints nothing. A mesh file the graph names is not watched: editing it does not
reload.

### Errors

A graph that cannot be read, a field-graph error, or a shader compile or link error goes to:

- the **status bar**, as one line: `<file>: field-graph error at <JSON pointer>: <message>`,
  plus `(keeping the previous scene)` when a scene is on screen;
- the **Log** dock, with a time stamp and the full message (a shader log is several lines).

The previous scene, and its image, stay on screen. Nothing repaints. The failing file
stays the open, watched one, so the next save that fixes it renders it. For the shorthand,
the pointer is a character offset.

## Window

| Part | Content |
|---|---|
| `File` menu | `Open…` (`Ctrl+O`), `Quit` (`Ctrl+Q`, the platform's Quit shortcut). |
| `View` menu | `Log`: show or hide the Log dock. |
| Status bar | The last load: `Loaded <file>: N nodes, M parameters`, or the last error. |
| Log dock | At the bottom, five lines high, read-only. One line per load, reload and error. It can be closed, floated or moved to another side. |

Menu shortcuts are Qt's standard key sequences, matched on the character typed, so they
follow the keyboard layout (Belgian AZERTY included).

## Mouse and keys

The same semantics as DualC's viewers (`OrbitState` in `examples/raymarch_gl`), plus pan.

| Input | Action |
|---|---|
| Left-drag | Orbit. Yaw −0.008 rad per pixel right, pitch +0.008 rad per pixel down. Pitch is clamped to ±1.5 rad. |
| Wheel | Dolly. Distance × 0.9 per notch away from you (closer), ÷ 0.9 per notch toward you. Floor 1e-4. |
| Middle-drag, or Shift + left-drag | Pan. The target slides in the view plane, and the point at the target follows the cursor. Yaw, pitch and distance are kept. |
| `F` (either case) | Frame the bounds (does nothing while no graph is on screen). The target goes to the trace box centre and the distance to 1.3 × its diagonal. Yaw and pitch are kept. |

Keys are matched by the character they type (`QKeyEvent::text()`), not by key position.
`F` is the key labelled F on any layout, Belgian AZERTY included. The viewport takes
keyboard focus when clicked.

### Preview and settle

While a drag is held, and for 0.18 s after the last camera change or button release, frames
are traced at `--preview-scale`. Then one full-resolution frame is drawn. After that the
viewport draws nothing until the next input, a resize or an expose. Holding a drag still
keeps the preview frame on screen and draws nothing new.

## Paint log

One line per `paintGL`, off by default:

```bash
QT_FORCE_STDERR_LOGGING=1 QT_LOGGING_RULES="dce.viewport.paint.debug=true" ./build/bin/DualCEditor
# paintGL #1 preview 300x200 30.5 ms
# paintGL #4 full 1200x800 145.2 ms
```

Each line gives the frame kind, the size actually traced, and the time of the frame. While
the log is on, each frame ends with `glFinish`, so the time includes the GPU. Use it to
check that an idle window prints nothing. `QT_FORCE_STDERR_LOGGING=1` is needed when stderr
is not a terminal, because Qt otherwise sends the log to journald.
