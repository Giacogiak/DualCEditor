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
| `graph` (positional) | `gyroid_box.json` | The graphs to open. Each graph becomes one object of a scene, in the order given, lowered to one shader (*Several graphs* below). In the window the first is opened and the others added (*Objects* below). In the window, a graph that does not load still opens the window, with the error in the status bar and the log, and is watched, so fixing it renders it. |
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
./build/bin/DualCEditor a.fld b.fld c.fld                 # a window on a scene of three objects
./build/bin/DualCEditor --preview-scale 0.25              # coarser, faster preview while moving
./build/bin/DualCEditor --snapshot out.png                # one frame, hardware GL
./build/bin/DualCEditor --snapshot out.png part.json      # one frame of part.json
./build/bin/DualCEditor --snapshot out.png a.fld b.fld c.fld   # a scene of three objects
LIBGL_ALWAYS_SOFTWARE=1 ./build/bin/DualCEditor --snapshot out.png   # Mesa llvmpipe
xvfb-run -s "-screen 0 1024x768x24" ./build/bin/DualCEditor --snapshot out.png  # no display (not yet run: xvfb is not installed on the dev box)
```

## Several graphs

With more than one `graph`, the window, `--snapshot` and `--bench` build a scene of one object per
graph and draw it through one shader (`../design/03-scene-lowering.md`). Each object keeps
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
| `File ▸ Open…` (`Ctrl+O`, the platform's Open shortcut) | File dialog filtered on `*.json *.fld`, with `All files` as the other filter. Starts in the last file's folder. **Replaces** the scene with one object. |
| `File ▸ Add…` (no shortcut) | The same dialog, several files allowed. **Adds** one object per file. |
| The `graph` arguments | The first is opened, the others added. |
| Drop files on the window | Accepted when the drag carries a local file ending in `.json` or `.fld` (any case). Other files are refused: the cursor shows no drop. **Adds** one object per graph file the drag carries, in order, to the scene on screen (the default `gyroid_box.json` included: delete it from the Objects dock). A drop anywhere on the window counts, the viewport, the docks and the log included. |

Opening or adding frames the camera on the scene's trace box (the union of every object's
box), like `F`. The window title is `<object name> - DualCEditor` with one object,
`N objects - DualCEditor` with several. An object is named after its file.

A relative mesh path (`mesh(path="cube.obj")`) is looked up beside the graph file first,
then in the working directory. `dualc_field_view` looks only in the working directory.

### Reload on save

Every object's file is watched (`QFileSystemWatcher`, the file and its folder). When one
changes on disk, it is re-read 0.1 s after the last change signal, and re-rendered only if its
text differs from the text last loaded. Its new graph replaces the graph of every object made
from it; the other objects keep their parameter edits, visibility and colour. Files that
change together reload as one. The camera is kept, as in `dualc_field_view`. Press `F`
to frame the new bounds. A save that renames a new file over the old one (as many editors
do) is followed too. A save with the same text, or a change to another file in the folder,
loads and paints nothing. A mesh file the graph names is not watched: editing it does not
reload.

### Errors

A graph that cannot be read, a field-graph error, or a shader compile or link error goes to:

- the **status bar**, as one line: `<file>: field-graph error at <JSON pointer>: <message>`,
  plus `(keeping the previous scene)` when a scene is on screen;
- the **Log** dock, with a time stamp and the full message (a shader log is several lines).

The previous scene, and its image, stay on screen. Nothing repaints. The failing file is
watched, so the next save that fixes it opens it (`File ▸ Open…`, first argument) or adds it
(a drop, `File ▸ Add…`, further arguments). A lowering error names the object:
`field-graph error at /root/in/1: object 2 (b.fld): unknown op 'no-such-op'`. For the shorthand,
the pointer is a character offset.

## Window

| Part | Content |
|---|---|
| `File` menu | `Open…` (`Ctrl+O`), `Add…`, `Quit` (`Ctrl+Q`, the platform's Quit shortcut). |
| `Edit` menu | `Delete Object`: removes the selected object. Greyed out with nothing selected. |
| `View` menu | `Objects`, `Parameters` and `Log`: show or hide each dock. |
| Status bar, left | The last load: `Loaded <file>: K objects, N nodes, M parameters` (the whole scene's counts, the per-object `transform` node and `visible`/`colour` uniforms included), `Removed <name>`, or the last error. |
| Objects dock | On the right, above Parameters. One row per object. See *Objects* below. |
| Status bar, right | Always shown: `compile C ms   frame F ms`, then `   preview P ms` once a preview frame has been timed. `-` means not measured yet. See *Timing* below. |
| Parameters dock | On the right. One editor per parameter of the scene on screen. See *Parameters* below. |
| Log dock | At the bottom, five lines high, read-only. One line per load (with its shader compile ms), reload and error. |

Each dock can be closed, floated or moved to another side. With both docks open, the default
1200×800 window leaves the viewport about 956×661.

Menu shortcuts are Qt's standard key sequences, matched on the character typed, so they
follow the keyboard layout (Belgian AZERTY included).

## Objects

The Objects dock lists the scene's objects in scene order (the order they were opened or
added). Each row is a visibility checkbox, a colour swatch and the object's name (its file
name; the tooltip gives the object id).

| Input | Effect |
|---|---|
| Click the checkbox (or `Space` on the selected row) | Hide or show the object. One uniform (`u_o<id>_visible`): no recompile, no relink, and showing it again gives the same image. A hidden object is not traced and not coloured; the trace box stays the same. |
| Click a row, arrow keys | Select that object. One at a time. The viewport highlights it (below). |
| Left click in the viewport | Select the object under the cursor: the nearest along the view ray where objects overlap on screen. A click on the background clears the selection. Its row is selected in this dock. Hidden objects are not picked. |

The selected object is tinted toward amber (mix 0.45 with (1.0, 0.78, 0.20), before the
1/2.2 gamma) and the other objects are dimmed to 0.45 of their colour; with nothing
selected every object has its own colour. Selecting sets one uniform (`u_selected`): no
recompile, no relink.
| `Delete` (`QKeySequence::Delete`, by meaning) with the dock focused, or `Edit ▸ Delete Object` | Remove the selected object. The scene is lowered and linked again (one link); the camera stays. The other objects keep their parameter edits. Deleting the last one leaves the viewport on the background. |

The swatch is the object's colour with the shader's 1/2.2 gamma: a lit face of the object is
about that colour. Colours follow the order objects were made in a scene: the first is
DualC's grey, then orange, blue, green, yellow, purple, teal, pink. `File ▸ Open…` starts a
new scene, so ids and colours start again. There is no colour picker yet: edit the
object's `colour` in the Parameters dock, and the swatch follows. Unchecking `visible`
there (0) checks the box off too.

## Parameters

The Parameters dock lists the binding table of the scene's shader (`GlslScene::bindings`):
every value that the generated shader reads from a uniform. They are grouped under their
object's name, then under their node, shown as its op and its pointer with the object's in
front (`onion /objects/1/root/in/0/in/1`). Each object ends with its own `object
/objects/<id>` group: `matrix` (the object's local-from-world transform), `visible` (1 or 0)
and `colour` (linear RGB). Each row is the parameter's name
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
dock. Adding or deleting another object keeps them. The trace step sizes stay those of the file's values: they come from the compiled graph,
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
| Left-drag | Orbit. Yaw −0.008 rad per pixel right, pitch +0.008 rad per pixel down. Pitch is clamped to ±1.5 rad. A drag starts once the cursor is the platform's drag distance (`QApplication::startDragDistance()`, 10 px by default, Manhattan) from the press; the whole movement then counts. |
| Left click (press and release within the drag distance, no Shift) | Select the object under the cursor, or clear the selection on the background (§ Objects). The view does not move. |
| Wheel | Dolly. Distance × 0.9 per notch away from you (closer), ÷ 0.9 per notch toward you. Floor 1e-4. |
| Middle-drag, or Shift + left-drag | Pan (from the same drag distance). The target slides in the view plane, and the point at the target follows the cursor. Yaw, pitch and distance are kept. |
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
`scripts/measure_viewport.py` runs it over the P00 reference scenes, and with `--objects`
over P01's scenes of 1 to 10 objects (it then also prints the ms per added object). Its
checklists are in `docs/roadmap/02-measurements.md`.

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
