# P00 — Spike A: DualC's sphere-tracer in a Qt viewport

**DONE (2026-10-12).** The question: can a `QOpenGLWidget` host DualC's generated tracer
with the same image and interactivity as `dualc_field_view`? Answer, on the GTX 1050 Ti:
the image matches, and the moving frame time is within about 10% on like-for-like rows
(T5). One graph at a time; several objects are P01 (`02-p01-scene-and-picking.md`).
Design: `../../design/01-render-path.md`, `../../design/02-graph-files.md`. Usage:
`../../command_reference/01-viewport.md`. Reference code in DualC:
`examples/field_glsl.{h,cpp}`, `examples/dualc_field_view.cpp`,
`examples/raymarch_gl.{h,cpp}`. Conditions of the development box: `README.md`.

## T1. Trace a field graph in the viewport

*Planned:* render DualC's `examples/samples/gyroid_box.json` through `compileToGlsl`,
`vertexShaderSource` and `assembleTraceShader`: a full-screen triangle, every framework and
binding uniform fed. Choose between porting DualC's GL helpers to
`QOpenGLFunctions_3_3_Core` (preferred, no glad or GLFW link) and reusing `raymarch_gl`
with glad through `QOpenGLContext::getProcAddress`. Add `--snapshot PATH` (one 800×600
frame to PNG). Acceptance: the app opens on the gyroid box shaded as `dualc_field_view`
shades it; `--snapshot` writes a PNG that is not one flat colour under llvmpipe; a ctest
runs it and reports skipped, not failed, without a GL 3.3 context;
`docs/design/01-render-path.md` exists and is indexed.

**DONE (2026-10-09, commit 19a6140).** Delivered: `TraceRenderer`
(`src/trace_renderer.{h,cpp}`: links vertex + trace shader at 330 core, full-screen
triangle on an empty VAO, feeds every `kTraceFramework` uniform, every binding and the mesh
grids, a port of `uploadGrid3D`); `compileGraphText` (`src/graph_scene`, dualc_field_view's
path: parse, `FieldGraph::build` bounds padded 5%, `compileToGlsl`); the `OrbitCamera`
maths (`src/orbit_camera`, a port of `OrbitState`'s `frameBounds` and `cameraFrame`);
`Viewport` drawing at `size × devicePixelRatio`, releasing GL objects on
`aboutToBeDestroyed`; `src/snapshot` with `--snapshot` (`QOffscreenSurface` +
`QOpenGLFramebufferObject`, a `QGuiApplication`, exit 77 when no GL 3.3 core context can
be made, the `offscreen` platform chosen when Linux has no display so xcb does not abort);
the default graph embedded with `qt_add_resources` from the DualC source tree
(`DUALCEDITOR_DUALC_SOURCE_DIR`, set for the fetched tree and for `DUALCEDITOR_DUALC_DIR`);
ctest `snapshot` (readable PNG, 800×600, at least 2 colours, at least 5% of pixels differ
from their row's left-edge pixel, `SKIP_RETURN_CODE 77`). Choice: the helpers ported to
`QOpenGLFunctions_3_3_Core`; `dualc_examples_gl` links glad and GLFW and only exists with
DualC's opt-in viewers. The cost is a copy to re-diff at each DualC pin bump (the files to
diff are in the design page).
Verified: our snapshot against `dualc_field_view --snapshot` at the pin, built in a scratch
tree: NV92 198 / 480 000 px differ, max channel diff 2; llvmpipe 246 px, all on silhouettes
(hit/miss flips), max 198; for scale, dualc_field_view itself differs between NV92 and
llvmpipe on 31 878 px, max 78. ctest `snapshot` passes on both drivers (0.9 s; llvmpipe 460
colours, surface 30.8% of the frame) and skips with `DISPLAY` unset. The live 1200×800
window renders the same view (checked with an `xwd` capture).
- *Wrong on the way:* the first snapshot was upside down. `toImage(false)` keeps GL's
  bottom-up rows; the default `toImage()` (flipped) is right. Caught because the shader's
  background gradient is lighter at the top.
- *Open then:* the `xvfb-run` path was never run (no xvfb on the box); under it `DISPLAY`
  is set, so the snapshot takes the xcb + GLX path tested here. On GitHub's ubuntu runner
  the GL tests skip; windows and macos runners were not tried.
- A reference `dualc_field_view` builds out of tree in about 10 minutes:
  `cmake -S build/_deps/dualc-src -B /tmp/ref -G Ninja -DDUALC_BUILD_EXAMPLES=ON -DDUALC_BUILD_FIELD_VIEW=ON -DDUALC_GC_DIR=$PWD/build/_deps/geometry-central-src`
  (it fetches GLFW).

## T2. Orbit camera and adaptive preview

*Planned:* mouse controls with `OrbitState`'s semantics (left-drag orbits, wheel dollies,
middle-drag or Shift+left-drag pans, `F` frames); while the camera moves, trace into an
offscreen target at a preview scale (default 0.5) and upscale; settle to full resolution
0.18 s after the last input; draw nothing new when idle. Acceptance: orbiting stays
responsive on llvmpipe at 0.25; the image settles after input stops; no continuous repaint
while idle, checked by logging `paintGL`; keys work on Belgian AZERTY, matched by meaning;
the command reference lists every key and mouse binding plus the preview flag.

**DONE (2026-10-10, commit 4c8a9e4).** Delivered: `orbitBy` and `dollyBy` with
raymarch_gl's constants (0.008 rad/px, pitch clamped ±1.5, `dist *= 0.9^steps`, floor
1e-4); `panBy`, new because `OrbitState` has no pan, sliding the target along the camera's
right and up by `2·dist·tan(22.5°)/viewHeight` per pixel so the point under the cursor
stays put; `kTanHalfFov` naming the 0.41421356 the renderer used inline. Viewport input:
left-drag orbits, middle or Shift+left pans, one wheel notch (`angleDelta().y()/120`) is
one GLFW scroll unit, `F` frames (target and distance reset, yaw and pitch kept), matched on
`QKeyEvent::text()` case-insensitively. `TraceRenderer::renderPreview` traces into a
private RGBA8 renderbuffer FBO at `int(fb × scale)` and blits it `GL_LINEAR` into the
widget's FBO (dualc_field_view's target and blit). The view counts as moving while a drag is
held and while a 180 ms `Qt::PreciseTimer` single-shot runs; every camera change and button
release restarts it; when it fires with no drag held and the last frame a preview, one
`update()` draws the full frame. Nothing repaints on a timer. `--preview-scale S` (default
0.5, clamped 0.1–1, 1 turns the preview off, a non-number exits 1). The
`dce.viewport.paint` logging category prints one line per `paintGL` (kind, traced size,
ms) and adds a `glFinish` per frame while on. Everything but `main.cpp` became the static
library `dualceditor_core` so tests can drive the widget. Tests: `orbit_camera` (GL-free:
framing, the constants, the clamp and floor, pan keeps the anchor under the cursor and F
after a pan restores the view) and `viewport` (0.25) / `viewport_no_preview` (1.0) on a real
800×600 widget with synthetic events: first frame a preview and the opening settles full;
no paint in 1 s idle; 20 drag moves give 20 preview-sized frames; a held still drag paints
nothing; after release exactly one full frame no sooner than 180 ms and within 1 s; one
wheel notch gives ×0.9; both pan gestures move the target and keep the angles; a key typing
`d` at F's QWERTY scan code does not frame, a key typing `f` with an unknown key code
frames, Shift+F frames. They skip (77) with no display, no GL 3.3 core, or the
`offscreen`/`minimal` platform.
Verified on the development box, paintGL wall time including `glFinish`, medians from
`viewport_test` on `gyroid_box.json`:

| GL | Window | Preview | Drag frame | Full frame |
|---|---|---|---|---|
| llvmpipe | 800×600 | 0.25 (200×150) | 24–29 ms | 120–124 ms |
| llvmpipe | 1280×800 | 0.25 (320×200) | 21 ms | 184–220 ms |
| llvmpipe | 1280×800 | 0.5 (640×400) | 56 ms | 178 ms |
| llvmpipe | 800×600 | off (1.0) | 104 ms | 107 ms |
| NV92 | 800×600 | 0.25 | 15–16.5 ms | 176 ms |
| NV92 | 800×600 | off (1.0) | 92 ms | 96 ms |

So "responsive on llvmpipe at 0.25" is about 40–50 frames/s dragging against 5–9 at full
resolution. The real binary on NV92 at 0.25, untouched 4 s: 3 preview frames (300×200),
one full frame at 0.41 s, then nothing.
- *Wrong on the way:* the settle timer first always called `update()`, so with the preview
  off it repainted an identical full frame 180 ms after every change, and press and release
  repainted an unchanged camera; now it repaints only when the last frame was a preview
  (`viewport_no_preview` covers it). The idle check after opening was flaky on llvmpipe:
  the window manager resizes the mapped window (a `Resize` and an `Expose` at 0.3–0.4 s),
  which repaints; the test now waits for the window to be exposed and paints to stop. A
  scripted edit of the test matched the wrong `runUntil` and cut a block; caught at compile
  time, nothing committed.
- *Open:* `Ctrl+F` and `Alt+F` do not frame (their `text()` is a control character or
  empty), deliberately, and no shortcut convention was chosen. HiDPI is unchecked (pan uses
  logical pixels, the preview device pixels). Touchpads send fractional `angleDelta`, so
  dolly steps are fractional. The paint log needs `QT_FORCE_STDERR_LOGGING=1` when stderr
  is piped, or Qt sends it to journald.
- **Owner check (2026-10-10), done:** "Now all camera controls work." While dragging on the
  Quadro, "blue-red-green squares around the cube, mostly on the boundaries between the
  shape and the background"; not fixed on the development box by the owner's direction
  (not the reference for rendering quality). T5 saw none on the GTX: a nouveau and Quadro
  matter.

## T3. Open a graph file, by menu or by drag and drop

*Planned:* accept `.json` and `.fld` graphs by `File ▸ Open`, by a command-line argument,
and by a drop; reload a file that changes on disk (`QFileSystemWatcher`); send a
`GraphError` (with its JSON pointer) or a shader error to the status bar and a log dock,
keeping the last good scene. Acceptance: a dropped `gyroid_box.json` or `.fld` renders;
saving the file re-renders it; a broken file shows its error and leaves the image; the
command reference documents the flags and the drop.

**DONE (2026-10-10, commit 7ff48f6).** Delivered: `MainWindow` (`src/main_window`):
`File ▸ Open…` (`QKeySequence::Open`, filter `*.json *.fld`), `File ▸ Quit`, `View ▸ Log`,
a bottom Log dock five lines high, the status bar; drops of local `.json`/`.fld` (any
case) anywhere on the window, other files refused at drag-enter; a failed open still makes
the file current and watched, so the save that fixes it renders it. The watch covers the
file and its folder; any signal restarts a 100 ms single-shot (`kReloadDelayMs`); on fire
the file is re-added if a rename-save dropped it, re-read, and recompiled only if the bytes
differ from the last text tried; the camera is kept (as dualc_field_view). Errors: the
first line to the status bar, with `(keeping the previous scene)` when a scene is on
screen; the full text with a time stamp to the Log; nothing repaints.
`Viewport::setScene(scene, frame)` with `sceneApplied` and `sceneFailed(log)`, a scene-less
constructor, a pending scene linked in `initializeGL`, a failed link keeping the old
program. `src/graph_file`: `loadGraphFile` / `compileGraphFileText` → `GraphLoad`; a
`.json` name forces JSON, otherwise the text is sniffed; relative mesh paths resolve beside
the graph first, then in the working directory (a superset of dualc_field_view's rule).
`main`: one positional `[graph]` (a second exited 1 until P01 T2), default
`:/samples/gyroid_box.json`; a missing or broken file exits 2 under `--snapshot` and opens
the window with the error otherwise. Test `main_window`: drops of `.json` and `.fld` render
and `.txt` is refused; in-place and write-and-rename saves re-render with the camera kept;
the file is still watched after the rename; a same-text save paints nothing; a broken JSON
shows `field-graph error at /root/in/1`, paints nothing, leaves the image pixel-identical;
a fix renders the identical image; a corrupted shader emits `sceneFailed` and keeps the
scene; `--snapshot` with `.fld` works and a broken argument exits 2; a relative
`mesh(path="cube.obj")` beside the graph resolves with the editor run elsewhere.
Verified on the development box: save to reload signal 150 ms on NV92, 110 ms on llvmpipe
(100 ms debounce + parse + compile + link); `main_window_test` about 32 s, mostly 500 ms
quiet waits and three `--snapshot` processes; the viewport is 1200×661 in the default
1200×800 window once the dock, menu and status bar take their room.
- *Wrong on the way:* the test waited on a child widget's `windowHandle()`, which a
  non-native child does not have; it waits on the top-level window's. The Log dock first
  opened at its default height (about 200 px, a 539 px viewport); `resizeDocks` sets five
  lines.
- *Open:* mesh files named by a graph are not watched, by design and as dualc_field_view
  (`../../design/07-mesh-import.md`; the owner met it on 2026-10-14, `../README.md`).
  Shorthand pointers are DualC's best-effort character offsets. A real file-manager drag,
  the Open dialog, saves from real editors (vim, gedit, VS Code, Notepad++) and the
  readability of the messages were never recorded as checked by a person.

## T4. Parameter dock: uniform-only edits

*Planned:* a dock listing `GlslScene::bindings`, one spin box or vector editor per
parameter; an edit updates a uniform only, never recompiles; the status bar shows the
shader compile time and the GPU frame time (`GL_TIME_ELAPSED`). Acceptance: changing the
gyroid wavelength or the onion thickness updates the image with no recompile, checked by
counting link calls; the status bar shows the compile ms and the ms of the last settled
frame.

**DONE (2026-10-10, commit 2676c6a).** Delivered: `ParameterPanel`
(`src/parameter_panel`), a `QScrollArea` in a right-hand Parameters dock
(`View ▸ Parameters`): a `float` gets one spin box, a `vec3` three, a `mat4` a 4×4
row-major grid; rows grouped under the node's op and pointer; adaptive decimal steps (a
tenth of the magnitude), range ±1e6, six decimals with trailing zeros hidden, keyboard
tracking off so a typed value applies on Enter or focus-out. `Viewport::setParameter` →
`TraceRenderer::setParameter` (`glUseProgram` + `setBinding`), never a link; `cameraChanged`
renamed `viewChanged`; an edit to the value already set paints nothing. The dock is rebuilt
on every `sceneApplied`, so a reload resets it to the file's values (superseded: P01 T2
carries values across a re-lowering, P03 T1 keeps edits in the document). `nodeOps`
(pointer → op) out of `compileGraphText`. `TraceRenderer::linkCount()` and `compileMs()`
(both stage compiles plus the link, to the `GL_LINK_STATUS` read). GPU time: each frame,
preview blit included, in a `GL_TIME_ELAPSED` query from a pool of 4; a 2 ms poll timer
runs only while queries are in flight and emits `gpuFrameTimed`; idle stays idle. Status
bar: `compile C ms   frame F ms` and `   preview P ms` once a preview is timed. Test
`parameter_dock`: one editor per binding at its value; the status bar matches `compileMs()`
and the last full-frame time; wavelength 1.0 → 0.6 and thickness 0.3 → 0.12 repaint,
change the image, leave `linkCount` unchanged; restoring gives the original image pixel for
pixel; a same-value edit paints nothing; a save with `wavelength 1.5` links exactly once;
`translate(sphere(center=…))` gives 3 + 16 editors and a vec3 edit changes the image with
no link. 7/7 ctests.
Verified on the development box: `compileMs` 81.2 ms NV92, 14.5 ms llvmpipe (900×600
window); settled frame 114.0 / 33.8 GPU ms. The query against the paint log's `glFinish`
wall time agrees within about 3 ms (NV92 full frame 106.0 wall / 103.4 GPU), except two
llvmpipe preview frames where the wall time counted waits outside the frame (51.8 against
30.7 ms).
- *Wrong on the way:* the design page first said the query and wall time agree within 3 ms
  on both drivers; the llvmpipe exception was added.
- *Open:* no person recorded checking the dock's feel (headings, label widths, a mat4 grid
  in a narrow dock, the step size), arrows on Belgian AZERTY, or the locale's decimal
  separator (`fr_BE` shows and expects `,`); the test drives `setValue`. The trace step
  constants (`stepScale`, `featureScale`) come from the compiled graph and do not follow an
  edit, so a wavelength much smaller than the file's may trace with artefacts until a
  recompile; recomputing them host-side needs DualC's per-node rule
  (`../06-dualc-questions.md`). DualC's binding table carries no limits, so a zero or
  negative wavelength is accepted. A DAG-deduplicated subtree has one binding under its
  first use, so one edit moves every copy (DualC's codegen). The dock takes width from the
  viewport (956×661 in the default window).

## T5. Measure on the GTX 1050 Ti — owner check

*Planned:* a measurement script or checklist; the owner runs it on the laptop (Windows:
the discrete GPU through `gpu_preference.h`; Linux PRIME:
`__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia`) on `gyroid_box.json`, a
tapered octet from DualC's `docs/command_reference/12-dualc_field_view/` and
`mesh_lattice.json`, at preview scales 0.35, 0.5 and 1.0 in a 1280×800 window.
Acceptance: `../02-measurements.md` has one row per scene and scale with moving and settled
ms, compile ms, commit, OS and driver; the handoff states whether the viewport matches
`dualc_field_view` on the same laptop (same image, frame time within about 10%). Readings
settled by the unit: "1280×800" is the traced size, the viewer comparison runs at its
1100×800 window; `mesh_lattice.json` names `cube.obj`, which DualC generates only at build
time, so the script writes the same cube; only moving frame times compare with the viewer,
which waits up to 0.1 s for input between settled redraws.

**DONE (2026-10-10, closed 2026-10-12; commit 53c6cb6).** Delivered: `--bench`
(`src/bench`): a bare `Viewport` traced at `--bench-size` (default 1280×800 device
pixels), 3 warm-up frames, then `--bench-frames` (60) *moving* frames (one orbit step per
presented frame) and 60 *settled* full-resolution frames; one JSON line with the median
`GL_TIME_ELAPSED` and the median wall ms between `frameSwapped` per phase, `compileMs`,
`firstFrameMs`, traced sizes, `GL_RENDERER` and `GL_VERSION`; vsync off (swap interval 0);
exit 1 on a link failure or a phase over 60 s, 2 when the graph does not load, 77 without
GL. `Viewport::orbitByPixels`, `glRenderer()`, `glVersion()`.
`scripts/measure_viewport.py`: runs the three scenes at the three scales, prints table
rows, appends raw JSON to `measurements-<date>.jsonl` (git-ignored), `--prime`,
`--field-view PATH` for the 800×600 pixel diff against the viewer (stdlib PNG decoder),
writes `cube.obj` (`makeCube`, side 1, DualC's vertex order) beside a copy of
`mesh_lattice.json`. ctest `bench` (160×100, 4 frames, regex on both phases and sizes).
Verified on the development box (tooling only): the full script on llvmpipe at
`--size 160x100 --frames 4` ran all nine runs (settled GPU ms gyroid 21, octet 135–145,
mesh lattice 59–60; the octet compiled 7.2 s the first time ever, 0.94–0.98 s from Mesa's
disk cache); NV92 gyroid bench at 640×400, scale 0.5: moving 17.0 GPU / 19.8 wall,
settled 48.9 / 51.8, compile 79 ms; the tapered octet does not run on NV92 (a 320×200
bench was still in its first frames after 2.5 min; the phase limit cannot fire while the
GPU is stuck in a frame); the PNG diff self-checks (0 px for two editor snapshots,
31 891 px NV92 against llvmpipe, T1 measured 31 878 with its own tool). 8/8 ctests.
**Owner's run (2026-10-10, GTX 1050 Ti, Windows 10, driver 441.93, Qt 6.8.3 msvc2022_64,
commit 53c6cb6, ctest 8/8):** all rows and the findings are in `../02-measurements.md`
(first table, § Against dualc_field_view, § Findings (P00 T5)); raw JSON
`../02-measurements-p00.jsonl`. Three rows of the pasted report were garbled and re-read
from the raw lines. In brief: the image matches (111 / 44 / 715 px of 480 000, max 1 / 1 /
20 per channel); the moving frame time is within 10% on the like-for-like 0.35 rows
(+10%, +6%, −2%), +15% on the two light 1.0 rows (+0.6 and +1.2 ms, a fixed per-frame
cost); the octet 1.0 row is not like for like (the viewer's window size unchecked); gyroid
and mesh_lattice are immediate in the window, the octet lags mostly at the start of a drag;
no coloured squares on the GTX.
- *Wrong on the way:* the first checklist said `dualc_field_view` draws continuously; when
  settled it waits up to 0.1 s for input before each redraw, so its "still" numbers are
  waits (the owner caught it; step 5 of the checklist compares drag frames only, with
  PresentMon on Windows). The checklist did not say the Windows build does not deploy Qt
  (`windeployqt`). A command-reference claim that llvmpipe takes "over 30 s" to the first
  octet frame was unmeasured (a whole-process cold-cache time) and removed. The first
  `--bench` filed GPU times by a broken counter and lost the last settled sample to a fixed
  100 ms wait; warm-up frames are now dropped by count and the run waits for every
  expected time, or 1 s.
- *Open:* the status bar's settled `frame` reads 7–12× high on light scenes in the live
  window (30 against 2.6 ms, gyroid) while the preview agrees; the owner's hypothesis, not
  verified, is that the GPU drops its clocks after the drag, before the one settled frame;
  a test would time one full frame after an idle gap in `--bench`. The light-scene
  composition cost (+0.6 to +1.2 ms at 1.0) is small; drawing to the window without
  `QOpenGLWidget`'s framebuffer would avoid it if many objects at high frame rates ever
  need it. The octet 1.0 comparison needs a re-run with the viewer's window at 1100×800.
  The tapered octet is not interactive on the reference GPU (8–12 fps at 0.35–0.5), nor
  in the viewer: input to P02. `--bench` on HiDPI is untested (Windows at 100% scaling).

## Exit criteria

T1 to T5 met as above; `python scripts/check.py` green (8/8 ctests on the development
box, 8/8 on the laptop); `../README.md` named P01.

---
← Back to the [Plan record](README.md) · [Roadmap index](../README.md).
