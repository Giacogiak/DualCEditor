# P00 — Spike A: DualC's sphere-tracer in a Qt viewport

This phase answers one question: can a `QOpenGLWidget` host DualC's generated tracer with
the same image and interactivity as `dualc_field_view`? It covers a single field graph.
Multiple objects come in P01.

Reference code in DualC:
- `examples/field_glsl.{h,cpp}`: `compileToGlsl`, `assembleTraceShader`,
  `vertexShaderSource`, `GlslScene::bindings`, and the `kTraceFramework` uniforms.
- `examples/dualc_field_view.cpp`: how the viewer feeds those uniforms, plus its preview
  scale, settle time and file watch.
- `examples/raymarch_gl.{h,cpp}`: the shared GL helpers and `OrbitState`.

### T1. Trace a field graph in the viewport

Render a fixed graph in `Viewport` through DualC's codegen, using
`examples/samples/gyroid_box.json` from DualC. The steps:
1. Compile the graph with `compileToGlsl`.
2. Build the program from `vertexShaderSource` and `assembleTraceShader`.
3. Draw a full-screen triangle.
4. Feed every uniform the trace framework and the bindings declare.

Choose how GL calls are made, and record the choice in a new `docs/design/01-render-path.md`:
- **Port the helpers to `QOpenGLFunctions_3_3_Core`.** This is preferred, since it adds no
  glad or GLFW link.
- **Reuse `raymarch_gl` with glad loaded through `QOpenGLContext::getProcAddress`.**

Add a `--snapshot PATH` flag that renders one 800×600 frame off-screen to a PNG and exits.

#### Acceptance criteria
- The app opens on the gyroid box, shaded the way `dualc_field_view` shades it.
- `DualCEditor --snapshot out.png` writes a PNG that is not a single flat colour, under
  Mesa llvmpipe (`LIBGL_ALWAYS_SOFTWARE=1`, or `xvfb-run` with no display).
- There is a ctest that runs `--snapshot` and checks the image. It reports skipped, not
  failed, when no GL 3.3 context can be made.
- `docs/design/01-render-path.md` exists and is indexed in `docs/design/README.md`.

### T2. Orbit camera and adaptive preview

Mouse controls, with the same semantics as `OrbitState`:
- Left-drag orbits.
- Wheel dollies.
- Middle-drag or Shift+left-drag pans.
- `F` frames the bounds.

While the camera moves, render into an offscreen target at a preview scale (default 0.5)
and upscale it. Settle to full resolution 0.18 s after the last input, as
`dualc_field_view` does. When idle, the widget draws nothing new.

#### Acceptance criteria
- Orbiting stays responsive on llvmpipe at a 0.25 preview scale.
- The image settles to full resolution after input stops.
- There is no continuous repaint while idle. Check this by logging paintGL calls.
- Keys work on a Belgian AZERTY layout: they are matched by meaning, never by position.
- `docs/command_reference/01-viewport.md` lists every key and mouse binding, plus the
  preview flag and its default. It is indexed in that folder's README.

### T3. Open a graph file, by menu or by drag and drop

Accept `.json` and `.fld` graphs in three ways:
- `File ▸ Open`
- a command-line argument
- dropping the file on the window

A file that changes on disk reloads, using `QFileSystemWatcher`. A `GraphError`, or a
shader compile error, goes to the status bar and a log dock, with its JSON pointer. The
last good scene stays on screen.

#### Acceptance criteria
- Dropping `gyroid_box.json` or a `.fld` file renders it.
- Saving the file in an editor re-renders it.
- A broken file shows its error and leaves the previous image in place.
- The command-reference page documents the new flags and the drop behaviour.

### T4. Parameter dock: uniform-only edits

A dock lists `GlslScene::bindings`, with one spin box or vector editor per parameter. An
edit only updates a uniform; it never recompiles. Show the shader compile time and the
GPU frame time in the status bar, measured with a `GL_TIME_ELAPSED` query.

#### Acceptance criteria
- Changing the gyroid wavelength or the onion thickness updates the image without a
  recompile. Check this by counting link calls.
- The status bar shows the compile time in ms, and the frame time in ms of the last
  settled frame.

### T5. Measure on the GTX 1050 Ti — human check

Prepare a measurement script or checklist for the owner. Then the owner runs it on the
laptop. On Windows, the discrete GPU is selected by `gpu_preference.h`. On Linux PRIME,
use `__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia`. Scenes to measure:
- DualC's `gyroid_box.json`
- a tapered octet from DualC's `docs/command_reference/12-dualc_field_view/`
- a `mesh` lattice (`mesh_lattice.json`)

Measure each at preview scales 0.35, 0.5 and 1.0, in a 1280×800 window.

Notes from the unit (2026-10-10):
- "1280×800" is the traced size: `--bench` shows a bare viewport of 1280×800 device
  pixels. The frame-time comparison with `dualc_field_view` runs at 1100×800, the size of
  its window.
- `mesh_lattice.json` names `cube.obj`, which DualC only generates at build time.
  `scripts/measure_viewport.py` writes the same cube beside a copy of the graph.
- Only moving frame times can be compared with `dualc_field_view`: when settled, it waits
  up to 0.1 s for input before each redraw, so its settled frame time is not a render time.

#### Acceptance criteria
- `docs/roadmap/02-measurements.md` has one row per scene and scale, with the moving and
  settled frame ms, the compile ms, and the commit, OS and driver.
- The handoff states whether the viewport matches `dualc_field_view` on the same laptop:
  the same image, and frame time within about 10%.

## Exit criteria
- Tasks T1 to T5 are met. `python scripts/check.py` is green.
- `docs/roadmap/README.md` § Current focus names P01.
