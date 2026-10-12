# 01 — Viewport, files and launch flags

`DualCEditor [options] [graph...]`

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
| `graph` (positional) | `gyroid_box.json` | The graph to open. The window takes one; a second exits 1. `--snapshot` and `--bench` take several: each graph becomes one object of a scene, in the order given, lowered to one shader (*Several graphs* below). In the window, a graph that does not load still opens the window, with the error in the status bar and the log, and is watched, so fixing it renders it. |
| `--snapshot PATH` | off | Render one 800×600 frame of `graph` (default view) off-screen to the PNG `PATH`, then exit. Opens no window. |
| `--preview-scale S` | `0.5` | Resolution factor while the view moves. The view is traced at `S` × the window's pixels, then upscaled linearly. It returns to full resolution 0.18 s after the last input. Clamped to 0.1 – 1. `1` turns the preview off. A value that is not a number exits 1. Same flag and default as `dualc_field_view`. |
| `--bench` | off | Time `graph` while orbiting and when settled, print one JSON line on stdout, and exit. See *Measurement run*. Vsync is off for the run. |
| `--bench-size WxH` | `1280x800` | Traced size of `--bench`, in device pixels. |
| `--bench-frames N` | `60` | Frames timed in each `--bench` phase, after 3 warm-up frames. |
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
./build/bin/DualCEditor --snapshot out.png a.fld b.fld c.fld   # a scene of three objects
LIBGL_ALWAYS_SOFTWARE=1 ./build/bin/DualCEditor --snapshot out.png   # Mesa llvmpipe
xvfb-run -s "-screen 0 1024x768x24" ./build/bin/DualCEditor --snapshot out.png  # no display (not yet run: xvfb is not installed on the dev box)
```

## Several graphs (`--snapshot`, `--bench`)

With more than one `graph`, `--snapshot` and `--bench` build a scene of one object per
graph and draw it through one shader (`../design/02-scene-lowering.md`). Each object keeps
its graph's own position: the transform is the identity, so graphs that overlap in space
overlap on screen (place them with `translate(...)` in the graph). Colours follow the order
of the arguments: the first is DualC's grey, then orange, blue, green, yellow, purple,
teal, pink, and round again. Every object is visible. The view is framed on the union of
the objects' trace boxes. A graph that does not read, parse or compile exits 2, and the
message names the object (`object 2 (b.fld): …`). In the `--bench` JSON, `graph` is the
paths joined by spaces.

```bash
printf 'translate(by=[-2,0,0],sphere(radius=0.8))' > a.fld
printf 'box(min=[-0.6,-0.6,-0.6],max=[0.6,0.6,0.6])' > b.fld
printf 'translate(by=[2,0,0],sphere(radius=0.8))' > c.fld
./build/bin/DualCEditor --snapshot three.png a.fld b.fld c.fld   # grey, orange, blue
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
| `View` menu | `Parameters` and `Log`: show or hide each dock. |
| Status bar, left | The last load: `Loaded <file>: N nodes, M parameters`, or the last error. |
| Status bar, right | Always shown: `compile C ms   frame F ms`, then `   preview P ms` once a preview frame has been timed. `-` means not measured yet. See *Timing* below. |
| Parameters dock | On the right. One editor per parameter of the graph on screen. See *Parameters* below. |
| Log dock | At the bottom, five lines high, read-only. One line per load (with its shader compile ms), reload and error. |

Each dock can be closed, floated or moved to another side. With both docks open, the default
1200×800 window leaves the viewport about 956×661.

Menu shortcuts are Qt's standard key sequences, matched on the character typed, so they
follow the keyboard layout (Belgian AZERTY included).

## Parameters

The Parameters dock lists DualC's binding table (`GlslScene::bindings`): every value of the
graph that the generated shader reads from a uniform. They are grouped under their node, shown
as its op and its pointer (`onion /root/in/0/in/1`). Each row is the parameter's name
(`thickness`, `wavelength`, `center`, `mat`). Its tooltip gives the uniform's type and name.

| Type | Editor |
|---|---|
| `float` | One spin box. |
| `vec3` | Three spin boxes: x, y, z. |
| `mat4` | A 4×4 grid, row-major, as the binding stores it. `translate`, `rotate` and `transform` nodes bind the local-from-world matrix. |

Using a spin box:

| Input | Effect |
|---|---|
| Up and down arrows, the wheel, the arrow buttons | Step the value and apply it at once. The step is a tenth of the value's magnitude (0.1 for 1.0, 0.01 for 0.3). Holding the key speeds it up. |
| Typing | The value applies on `Enter` or when the box loses focus, not at every character. |
| Page Up and Page Down | Ten steps. |

The range is −10⁶ to 10⁶, with up to six decimals. No other bound is enforced: a zero
wavelength or thickness gives what the shader computes.

An edit only sets the uniform. The shader is not recompiled or relinked, so it shows at once.
The view previews and settles as for a camera move. Edits are not written to the file. A
reload of the file, or opening another one, brings back the file's values and rebuilds the
dock. The trace step sizes stay those of the file's values: they come from the compiled graph,
as in `dualc_field_view`. A wavelength much smaller than the file's can trace with visible
artefacts. Save it in the file to recompute them.

Keys typed into a spin box go to it, not to the viewport. Click the viewport to give it back
the keys (`F`).

## Timing

| Field | Measured as |
|---|---|
| `compile` | Wall time of the GL compile of both shader stages plus the program link, for the scene on screen. DualC's parse and codegen are not included. It also goes to the Log at each load. |
| `frame` | GPU time of the last full-resolution (settled) frame, from a `GL_TIME_ELAPSED` query. |
| `preview` | GPU time of the last preview frame, with the upscale blit. |

The query is read a few milliseconds after the frame, without stalling. While idle, nothing
is measured and the values stay.

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

## Measurement run

`DualCEditor --bench [--preview-scale S] [--bench-size WxH] [--bench-frames N] [graph]`
shows a bare viewport (no menu, docks or status bar) whose traced size is `WxH` device
pixels, then runs two phases of `N` frames each:

| Phase | Frames |
|---|---|
| `moving` | The camera orbits one drag pixel per presented frame, so the view previews at `S`, as under a held drag. |
| `settled` | Full-resolution frames of a still camera, one per presented frame. |

It prints one JSON object, then exits 0:

| Field | Meaning |
|---|---|
| `moving.gpuMs`, `settled.gpuMs` | Median `GL_TIME_ELAPSED` time of a frame. A preview frame includes its upscale. |
| `moving.wallMs`, `settled.wallMs` | Median time between presented frames (`frameSwapped`): the trace, Qt's composition and the swap. |
| `…gpuFrames`, `…wallFrames`, `…traced` | Samples behind each median, and the size traced. |
| `compileMs` | GL compile + link of the trace shader. Drivers cache shaders on disk: on llvmpipe the tapered octet linked in 7.2 s the first time and 0.95 s from Mesa's cache. |
| `firstFrameMs` | Wall time from showing the window to the first presented frame. Some drivers finish compiling at the first draw, so it can be much longer than `compileMs`. |
| `glRenderer`, `glVersion`, `qt`, `version` | What ran it. On NVIDIA, `glVersion` carries the driver version. |

Exit 1 when the shader does not link or a phase takes over 60 s, 2 when the graph does not
load, 77 with no GL 3.3 core context (on Linux with no display, as for `--snapshot`).
`scripts/measure_viewport.py` runs it over the P00 reference scenes. Its checklist is in
`docs/roadmap/02-measurements.md`.

```bash
./build/bin/DualCEditor --bench --preview-scale 0.35 part.json
# {"compileMs":79.41,"firstFrameMs":…,"moving":{"gpuFrames":60,"gpuMs":17,…},"settled":{…},…}
```

## Paint log

One line per `paintGL`, off by default:

```bash
QT_FORCE_STDERR_LOGGING=1 QT_LOGGING_RULES="dce.viewport.paint.debug=true" ./build/bin/DualCEditor
# paintGL #1 preview 478x330 65.7 ms
# gpu preview 478x330 65.0 ms
# paintGL #4 full 956x661 110.4 ms
# gpu full 956x661 107.8 ms
```

Each `paintGL` line gives the frame kind, the size actually traced, and the time of the frame.
Each `gpu` line is the `GL_TIME_ELAPSED` time of the same frame, logged when the query is read. While
the log is on, each frame ends with `glFinish`, so the time includes the GPU. Use it to
check that an idle window prints nothing. `QT_FORCE_STDERR_LOGGING=1` is needed when stderr
is not a terminal, because Qt otherwise sends the log to journald.
