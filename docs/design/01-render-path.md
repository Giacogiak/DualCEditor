# 01 — Render path

How a field graph reaches the screen. Born in P00 T1 (2026-10-09).

## Choice: DualC's helpers ported to `QOpenGLFunctions_3_3_Core`

The plan offered two ways to make GL calls:

- **Port the helpers to `QOpenGLFunctions_3_3_Core`** — chosen.
- Reuse DualC's `raymarch_gl` with glad loaded through `QOpenGLContext::getProcAddress` —
  not taken.

Why: `dualc_examples_gl` links GLFW and glad publicly and is only configured when one of
DualC's opt-in GL viewers is on. Linking it would add a GLFW fetch and a second GL loader
for a few dozen lines: shader compile and link, the uniform setters, the 3D grid upload,
and the orbit camera maths. `QOpenGLFunctions_3_3_Core` resolves every entry point from
the current Qt context, so no loader runs. The cost is that the port is a copy. If DualC
changes `uploadGrid3D` or the uniform feed of `dualc_field_view`, the editor does not
follow until someone ports the change. The DualC files to diff against at a pin bump are
`examples/raymarch_gl.cpp` (`uploadGrid3D`, `frameBounds`, `orbitCamera`) and
`examples/dualc_field_view.cpp` (`drawFrame`, `setBinding`).

## Pieces

| File | Role |
|---|---|
| `src/graph_scene.{h,cpp}` | `compileGraphText`: graph text → `GlslScene`. It parses JSON or shorthand, takes the field's own bounds (built once through `FieldGraph::build`) padded by 5% as the trace box, then calls `compileToGlsl`. This is dualc_field_view's path without `--bounds`. Throws `GraphError`. |
| `src/trace_renderer.{h,cpp}` | `TraceRenderer`: links `vertexShaderSource(false)` + `assembleTraceShader(scene, false)` (GLSL 330 core), uploads the mesh grids to `GL_R32F` 3D textures, sets the bindings, and draws a full-screen triangle (`glDrawArrays(GL_TRIANGLES, 0, 3)` on an empty VAO). It holds no window and no context. |
| `src/orbit_camera.{h,cpp}` | `OrbitCamera`, `frameBounds`, `cameraFrame`: DualC's `OrbitState` maths with the same constants (yaw 0.7, pitch 0.5, distance 1.3 × diagonal), plus the input steps `orbitBy`, `dollyBy` (raymarch_gl's `orbitDragTo`, `orbitDolly`) and the editor's own `panBy`. |
| `src/viewport.{h,cpp}` | `Viewport` (`QOpenGLWidget`): owns a `TraceRenderer` and draws into the widget's FBO at `size × devicePixelRatio`, at preview scale while the view moves (below). Turns mouse and key events into camera steps. Takes scenes through `setScene` (`02-graph-files.md`). GL objects are released on `aboutToBeDestroyed`. |
| `src/snapshot.{h,cpp}` | `renderSnapshot`: a `QOpenGLContext` + `QOffscreenSurface` + `QOpenGLFramebufferObject`, one `render`, `toImage()`, saved as PNG. No window is created. |

Both the widget and the snapshot draw through the same `TraceRenderer::render`, so the
`--snapshot` ctest checks the code path the window uses.

## Uniform feed

`TraceRenderer::render` sets every uniform in `kTraceFramework`, each from the same
expression `dualc_field_view` uses:

| Uniform | Value |
|---|---|
| `uCamPos`, `uCamForward`, `uCamRight`, `uCamUp` | `cameraFrame(orbit)` |
| `uTanHalfFov` | `tan(22.5°)` = 0.41421356 |
| `uAspect` | width / height of the target |
| `uBoundsMin`, `uBoundsMax` | `scene.bounds` |
| `uLightDir` | normalize(0.5, 0.85, 0.6) |
| `uStepScale` | `scene.stepScale` |
| `uMetricSDF` | 1 if `stepScale ≥ 0.999`, else 0 |
| `uStepMin`, `uHmem` | min(diag × 0.002, feat × 0.003) |
| `uStepMax` | min(diag × 0.100, feat × 0.350) |
| `uClip[3]`, `uClipMask` | from `TraceRenderer::SectionPlanes`, built each frame from the bounds as dualc_field_view does (zeros, 0 with no section on: the image is then exactly the unclipped one). The window's Section toolbar sets them (`docs/command_reference/01-viewport.md` § Section). |

Here `diag` is the diagonal of the trace box, and `feat` is `scene.featureScale` clamped
to `diag` (or `diag` itself when it is 0). Each `GlslScene::bindings` entry goes up once
per `setScene`: `float` → `glUniform1f`, `vec3` → `glUniform3f`, `mat4` →
`glUniformMatrix4fv(…, GL_TRUE, …)` (row-major, as in DualC). Each mesh grid takes texture
unit *i* plus the `<base>_texMin/_texScale/_texDim` uniforms.

## Adaptive preview (P00 T2)

This follows dualc_field_view's render loop, moved onto Qt's event-driven painting.

- **Moving.** The view counts as moving while a drag is held, and for `kSettleMs` = 180 ms
  after the last camera change (move, wheel, `F`) or button release. A `Qt::PreciseTimer`
  single-shot `settle_` marks that window. A frame painted while moving goes through
  `TraceRenderer::renderPreview`. It traces at `int(fb × scale)` into a private RGBA8
  renderbuffer FBO, then `glBlitFramebuffer`s it with `GL_LINEAR` into
  `defaultFramebufferObject()`. This is the same target and blit as dualc_field_view.
- **Settling.** When `settle_` fires with no drag held, and the last frame was a preview,
  the widget calls `update()` once. That frame is full resolution.
- **Idle.** Nothing calls `update()` on a timer, so an idle widget runs no `paintGL`. Qt
  still repaints on a resize or on the first expose. dualc_field_view instead wakes every
  0.1 s for its file watch, but it skips the draw.
- **Where it differs from dualc_field_view.** dualc_field_view spins at vsync while a drag is
  held, even with the mouse still. Here a frame is painted only when the camera changes.
  A press or release that does not move the camera paints nothing. A release only starts
  the settle wait. With `--preview-scale 1` there is no extra settle frame.
- **The opening.** `initializeGL` starts `settle_`, so the first frame is a preview, as in
  dualc_field_view.

`viewport_test` checks this with synthetic events. The paint log is the
`dce.viewport.paint` logging category (`docs/command_reference/01-viewport.md`).

## Parameter edits and timing (P00 T4)

DualC's codegen has two edit tiers. A structural change recompiles. A change to a value in
`GlslScene::bindings` only sets a uniform. The Parameters dock is the second tier.

- **The path of an edit.** `ParameterPanel` (`src/parameter_panel.{h,cpp}`) emits
  `parameterEdited(index, value)` with the binding's whole value. `MainWindow` connects
  it to `Viewport::setParameter`. That writes the value into the scene on screen
  (`scene_.bindings`), makes the context current and calls `TraceRenderer::setParameter`,
  which is `glUseProgram` plus the same `setBinding` that `setScene` uses. Then the view
  previews and settles as for a camera move, as dualc_field_view does for its `[`/`]`
  nudge. An edit to the value already set does nothing.
- **What an edit does not touch.** `stepScale`, `featureScale` and the trace box are
  computed by the codegen from the graph's values, and stay as compiled. dualc_field_view
  does the same. So a much smaller wavelength than the file's keeps the file's step sizes.
  An edit is not written to the file. The next `setScene` (a reload or an open) brings
  the file's values back, and the dock is rebuilt from them on `sceneApplied`.
- **Counting links.** `TraceRenderer::linkCount` counts `glLinkProgram` calls.
  `parameter_dock_test` checks that edits leave it unchanged and that a changed save adds
  one.
- **Compile time.** `TraceRenderer::setScene` (and `linkProgram` on the compile thread) times the vertex and fragment compile and
  the link with a `QElapsedTimer`, up to the `GL_LINK_STATUS` read, which waits for the
  link. It is GL time only: DualC's parse and codegen are not in it.
- **GPU frame time.** `paintGL` wraps each frame (the preview blit included) in a
  `GL_TIME_ELAPSED` query. `TraceRenderer` owns a pool of `kGpuTimers` = 4 queries. A
  frame painted while all 4 are in flight is not timed. The result is not read in
  `paintGL`, because that would stall. A 2 ms `gpuPoll_` timer runs only while queries
  are in flight. It reads the finished ones in order, without blocking, and emits
  `gpuFrameTimed(preview, traced, ms)`. It stops when none are left, so an idle widget
  still does nothing. The status bar shows the last full-resolution (settled) frame and
  the last preview frame.

## Asynchronous compile (P07 T2, closes E-21)

A structural edit (open, add, delete, reload) no longer blocks the GUI thread.

- **Two threads, one share group.** `Viewport::buildScene(make, frame, finish)` hands the
  edit to a compile thread (`Viewport::Compiler`, a `QThread`). That thread owns a
  `QOpenGLContext` made with `setShareContext(widget context)`, current on a
  `QOffscreenSurface`, both created on the GUI thread when the first build starts. On it,
  `make` runs DualC's codegen (`MainWindow` passes `lowerScene`, mesh bakes included), then
  `TraceRenderer::linkProgram` compiles and links, timed as before, then `glFinish`, so the
  widget's context only ever sees a finished program. The result goes back to the GUI
  thread as a queued call.
- **The swap.** On the GUI thread, `finish` may edit the binding values (the window's
  `carryValues`: parameter edits and Objects-dock flags made while the scene compiled).
  Then `TraceRenderer::adoptProgram` takes the program, uploads the mesh grids and every
  binding, and sets the trace constants, all between two frames. A frame therefore draws
  the old program with the old values, or the new program with the new values, never a mix.
  Until then the widget keeps drawing the old program; camera moves and parameter edits
  work on it.
- **The last edit wins.** Every scene change (`buildScene`, `setScene`, `clearScene`)
  takes a new ticket. The thread takes only the newest build waiting. A result whose ticket
  is not the last build's is dropped and its program deleted. `MainWindow::apply` builds
  each edit on the doc still compiling (`latest()`), so an add during a compile gives a
  scene with both objects. The replaced edit's load attempts end with the last one's
  `loadFinished`.
- **Failures.** A codegen exception or a compile or link log comes back as `sceneFailed`;
  the old program, grids and image stay, as before. The window reports a lowering error as
  a field-graph error and the rest as a shader error, and watches the files of every add
  merged into the failed edit.
- **Context changes.** `releaseGL` stops the thread, which first finishes any link under
  way, since a GL link cannot be cut short. A build in flight starts again on the next
  context, and results linked on the old context are not touched.
- **What stays synchronous.** `Viewport::setScene` still links on the GUI thread. `--bench`
  uses it, so its `compileMs` measures what it did before. `--snapshot` uses
  `TraceRenderer::setScene`. The reparenting relink also stays synchronous.
- **No program cache.** The driver's disk cache already brings a source seen before to
  14–31 ms on the GTX (`../roadmap/02-measurements.md`). An editor-side cache is not built.

`async_compile_test` checks the swap image by image while frames are drawn, the last of two
builds, both failures, and the window's add during a compile (llvmpipe and NV92,
2026-10-13).

## Measurement run (P00 T5)

`src/bench.{h,cpp}` drives a bare `Viewport` for `--bench`. It steps the camera through
`Viewport::orbitByPixels`, the same path as a mouse drag, once per `frameSwapped`, so the
next frame is drawn as soon as the last one is presented. The default surface format gets
swap interval 0 for the run only. GPU times are filed under the phase they arrive in; the
phases are separated by three settle times, much longer than a query takes to come back.
Usage and output: `../command_reference/01-viewport.md` § Measurement run.

## Context

`main` sets the default `QSurfaceFormat` to 3.3 core before any surface exists.
`TraceRenderer::initialize` refuses a context that is ES or older than 3.3. The snapshot
then exits 77, which the ctest reports as skipped.

`--snapshot` uses a `QGuiApplication`, with no widgets. On Linux, when neither `DISPLAY` nor
`WAYLAND_DISPLAY` is set and `QT_QPA_PLATFORM` is unset, it selects the `offscreen`
platform. Without that, xcb would abort the process before the code could report "no GL".
The offscreen platform has no GL context without an X display, so the snapshot exits 77.
On a box with a display, or under `xvfb-run`, the snapshot uses the normal platform and a
hidden off-screen surface.

## Verified (2026-10-09, P00 T1)

- The 800×600 `--snapshot` of `gyroid_box.json` matches `dualc_field_view --snapshot` at
  the pinned commit. On nouveau, 198 of 480 000 pixels differ, by at most 2 levels. On
  llvmpipe, 246 pixels differ, all silhouette pixels where hit and miss flip. For scale,
  DualC's own hardware and llvmpipe images differ on 31 878 pixels. Conditions are in
  `HANDOFF_P00_TASK1.md`.
- On this development box (Quadro FX 3700, nouveau NV92, Mesa 26.0.8): hardware and
  `LIBGL_ALWAYS_SOFTWARE=1` (llvmpipe, LLVM 21.1.8) both render. With `DISPLAY` unset,
  the snapshot exits 77 and the ctest reports skipped.

## Verified (2026-10-10, P00 T2)

- `viewport_test` passes on nouveau and on llvmpipe, with the preview at 0.25 and at 1.0.
  It checks the preview size of each drag frame, one full frame no sooner than 180 ms
  after release, and no `paintGL` in 1 s of idle. The frame times are in
  `HANDOFF_P00_TASK2.md`. They come from a development box, so they are not performance
  figures (E-04).

## Verified (2026-10-10, P00 T4)

- `parameter_dock_test` passes on nouveau and on llvmpipe. Edits of the gyroid wavelength,
  the onion thickness and a vec3 component change the image with no link, and putting the
  file's values back gives the original image pixel for pixel.
- On both drivers, the GPU timer query mostly agrees with the `glFinish` wall time of the
  paint log, within about 3 ms. Once on llvmpipe, a preview frame read 30.7 ms against a
  51.8 ms wall time. The wall time also counts waits outside the frame. Numbers and
  conditions are in `HANDOFF_P00_TASK4.md`.
