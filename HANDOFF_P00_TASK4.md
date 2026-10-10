# Handoff — P00 T4: parameter dock, uniform-only edits

Date: 2026-10-10. Plan: `plan/10_P00_viewport.md` § T4. DualC pin: `2fcb6ee` (unchanged).

## What changed and why

- **`ParameterPanel`** (`src/parameter_panel.{h,cpp}`, new). It is a `QScrollArea` in a
  right-hand `Parameters` dock, shown and hidden from `View ▸ Parameters`. It has one row
  per `GlslScene::bindings` entry:
  - a `float` gets one spin box, a `vec3` three, a `mat4` a 4×4 grid (row-major, as
    stored);
  - rows are grouped under a heading with the node's op and pointer, for example
    `onion /root/in/0/in/1`;
  - the row label is `paramKey`, and the tooltip gives the uniform's type and name.

  The spin boxes:
  - use adaptive decimal steps, a tenth of the value's magnitude, which is scale-free like
    dualc_field_view's multiplicative nudge;
  - have a range of ±1e6 and six decimals, with trailing zeros hidden;
  - have keyboard tracking off, so a typed value applies on Enter or on focus-out (`0.05`
    does not pass through `0`).

  An edit emits `parameterEdited(index, value)` with the binding's whole value.
- **`Viewport::setParameter(index, value)`**. It writes the value into `scene_.bindings`
  and calls `TraceRenderer::setParameter`, which is `glUseProgram` plus the existing
  `setBinding`. It never compiles or links. The view then previews and settles like a
  camera move, as dualc_field_view does for its nudge. `cameraChanged` was renamed
  `viewChanged` for that reason. An edit to the value already set paints nothing.
- **The dock follows the scene.** It is rebuilt on every `sceneApplied`, from the new
  scene's bindings. A reload therefore resets the dock to the file's values. Edits are not
  written back.
- **Op names.** `compileGraphText` takes an optional `nodeOps` out map (pointer → op), and
  `GraphLoad::nodeOps` carries it. `MainWindow` keeps the map of the scene being loaded
  and moves it to the dock on `sceneApplied`.
- **Link count and compile time.** `TraceRenderer::linkCount()` counts `glLinkProgram`
  calls. `compileMs()` is the wall time of both stage compiles plus the link, up to the
  `GL_LINK_STATUS` read. Both are exposed on `Viewport`.
- **GPU frame time.** `paintGL` wraps each frame, preview blit included, in a
  `GL_TIME_ELAPSED` query from a pool of 4 owned by `TraceRenderer`. A 2 ms `gpuPoll_`
  timer runs only while queries are in flight. It reads the finished ones without
  blocking and emits `Viewport::gpuFrameTimed(preview, traced, ms)`. The paint log
  (`dce.viewport.paint`) gains a `gpu …` line per timed frame. Idle stays idle: the poll
  stops with no query in flight and never paints.
- **Status bar.** A permanent right-hand label shows `compile C ms   frame F ms`, then
  `   preview P ms` once a preview frame has been timed. `frame` is the last
  full-resolution (settled) frame. The Log line of each load now carries the compile ms.
- **Test `parameter_dock`** (`tests/parameter_dock_test.cpp`). It runs on a live
  `MainWindow` with the preview off and checks:
  - the dock has one editor per binding float, at its value;
  - the status bar matches `compile X ms   frame Y ms`, with X = `compileMs()` and Y = the
    last full-frame `gpuFrameTimed`;
  - a wavelength edit (1.0 → 0.6) and a thickness edit (0.3 → 0.12) each repaint, change
    the image, and leave `linkCount` unchanged;
  - the frame ms follows the new settled frame;
  - restoring 1.0 and 0.3 gives the original image pixel for pixel, still with no link;
  - a same-value edit paints nothing;
  - a save with `wavelength 1.5` links exactly once, and the dock shows 1.5;
  - a `.fld` with `translate(sphere(center=…))` gives 3 editors for the vec3 and 16 for
    the mat4, and a vec3 component edit changes the image with no link.

  It skips (77) like `main_window`.
- **Docs.**
  - `docs/command_reference/01-viewport.md`: the Window table, and new *Parameters* and
    *Timing* sections. The paint log example now shows the `gpu` lines.
  - `docs/design/01-render-path.md`: a new section, *Parameter edits and timing*, and a
    *Verified* entry.
  - The design and command-reference READMEs, and the roadmap's current focus.

## Measured (development box, not the reference GPU)

Box: Quadro FX 3700 on nouveau (NV92), Mesa 26.0.8, Qt 6.10.2, Ubuntu, X11, Release build.
llvmpipe means `LIBGL_ALWAYS_SOFTWARE=1` (LLVM 21.1.8). Scene: `gyroid_box.json`.

- Shader compile + link (`compileMs`): 81.2 ms on NV92 and 14.5 ms on llvmpipe, from
  `parameter_dock_test` in a 900×600 window.
- Settled frame GPU time, from the status bar in the same test: 114.0 ms on NV92 and
  33.8 ms on llvmpipe. The viewport is smaller than 900×600 there, because the docks take
  their share.
- GPU query against the paint log's `glFinish` wall time, real binary, default 1200×800
  window. The viewport is 956×661 and the preview 478×330. Each pair is wall / GPU, in ms:

  | Frame | NV92 | llvmpipe, run 1 | llvmpipe, run 2 |
  |---|---|---|---|
  | preview #1 | 65.7 / 65.0 | 90.3 / 89.6 | 77.2 / 76.5 |
  | preview #2 | 36.6 / 34.7 | 51.8 / 30.7 | 57.6 / 56.7 |
  | preview #3 | 66.3 / 64.8 | 44.8 / 36.4 | 44.7 / 44.1 |
  | full | 106.0 / 103.4 | 132.2 / 131.8 | 127.8 / 127.4 |

  The two agree within about 3 ms, except on two llvmpipe preview frames in run 1, where
  the wall time counts waits outside the frame.
- `parameter_dock_test` takes about 10 s. The full gate (`python scripts/check.py`) passes,
  with 7/7 ctests.

## What I got wrong on the way

- I first wrote in the design page that the query and the wall time agree within 3 ms on
  both drivers. A second look at the first llvmpipe run showed a preview frame at
  51.8 ms wall against 30.7 ms GPU. The page now states that exception.

## Open

- **A human should look** (needsHumanCheck). Not covered by the tests:
  - how the dock reads and feels: the headings, the label widths, a 4×4 mat4 grid in a
    narrow dock, and whether the adaptive step is a good size for real edits;
  - that typing and the arrow keys behave on Belgian AZERTY. The decimal separator follows
    the system locale: a `fr_BE` locale shows and expects `,`;
  - that the image updates live while an arrow is held, and that the status-bar numbers
    look plausible.

  The test drives `setValue` directly, never real key or mouse input on the spin boxes.
- **The dock takes width from the viewport.** In the default 1200×800 window, the viewport
  is about 956×661, against 1200×661 in T3. T5 measures in a 1280×800 window, and the
  owner may want to close the dock, or count the viewport size, when measuring.
- **Trace step constants do not follow edits.** `stepScale` and `featureScale` (step
  min/max, FD step) come from the compiled graph, as in dualc_field_view. A wavelength
  much smaller than the file's may trace with artefacts until the file is saved with it.
  Recomputing `featureScale` host-side would need DualC's per-node feature rule
  (`nodeFeatureScale`), which is not exported. That is a DualC question if wanted.
- **Edits are lost on reload,** and are not written to the file. Keeping edits across a
  reload, matched by uniform name and pointer, or saving them, is a later decision.
- **No range per parameter.** DualC's binding table carries no limits, so a zero or
  negative wavelength or thickness is accepted.
- **A DAG-deduplicated subtree** has one binding, under the pointer of its first use, so
  one edit moves every copy. That is DualC's codegen behaviour.

## For the next unit (T5)

- Read the numbers from the status bar (`compile`, `frame` for settled, `preview` for
  moving), or from the paint log with
  `QT_FORCE_STDERR_LOGGING=1 QT_LOGGING_RULES="dce.viewport.paint.debug=true"`. Its `gpu`
  lines are the `GL_TIME_ELAPSED` times. Turning the log on also adds a `glFinish` per
  frame, which the GPU time does not include.
- The compile time is GL compile + link only. dualc_field_view prints no compile time, so a
  comparison needs its own timing. DualC's parse and codegen are not in the editor's
  number either.
- The viewport size depends on the docks. Close the Parameters and Log docks, or record
  the viewport size, to get a true 1280×800 viewport. The traced size is in the `gpu`
  paint-log lines.
