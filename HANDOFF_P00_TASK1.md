# Handoff — P00 T1: trace a field graph in the viewport

Date: 2026-10-09. Plan: `plan/10_P00_viewport.md` § T1. DualC pin: `2fcb6ee`.

## What changed and why

- **Render path: DualC's helpers ported to `QOpenGLFunctions_3_3_Core`** (the plan's
  preferred option). This adds no glad or GLFW link: `dualc_examples_gl` links both and is
  only configured with DualC's opt-in GL viewers. The cost is a copy that has to be
  re-diffed at each DualC pin bump. The reasoning and the files to diff are in
  `docs/design/01-render-path.md`.
- `src/trace_renderer.{h,cpp}` — `TraceRenderer`. It links `vertexShaderSource` +
  `assembleTraceShader` (330 core), draws a full-screen triangle on an empty VAO, and
  feeds every `kTraceFramework` uniform and every `GlslScene::bindings` entry, plus the
  mesh grids (a port of `uploadGrid3D`). The uniform table is in the design page.
- `src/graph_scene.{h,cpp}` — `compileGraphText`, which follows dualc_field_view's
  path: parse, take `FieldGraph::build` bounds padded by 5%, then `compileToGlsl`.
- `src/orbit_camera.{h,cpp}` — a port of DualC's `OrbitState` maths (`frameBounds`,
  `cameraFrame`). It has no input handling yet; that is T2.
- `src/viewport.{h,cpp}` — the widget now takes a `GlslScene` and draws through
  `TraceRenderer` at `size × devicePixelRatio`. It releases its GL objects on
  `aboutToBeDestroyed`.
- `src/snapshot.{h,cpp}` + `--snapshot PATH` in `src/main.cpp` — one 800×600 frame through
  `QOffscreenSurface` + `QOpenGLFramebufferObject`, saved as PNG. It exits 77 when no GL
  3.3 core context can be made. A snapshot builds a `QGuiApplication`, not a
  `QApplication`. On Linux with no display it selects the `offscreen` platform, so it
  reports 77 instead of xcb aborting.
- The default graph is DualC's `examples/samples/gyroid_box.json`, embedded with
  `qt_add_resources` from the DualC source tree (`DUALCEDITOR_DUALC_SOURCE_DIR`, set for
  both the fetched tree and `DUALCEDITOR_DUALC_DIR`).
- `tests/snapshot_test.cpp` + ctest `snapshot`. It runs the editor with `--snapshot` and
  checks: a readable PNG, 800×600, at least 2 colours, and at least 5% of pixels differing
  from their row's left-edge pixel. The background is a vertical gradient, so those
  pixels are traced surface. `SKIP_RETURN_CODE 77`.
- Docs: `docs/design/01-render-path.md` (new, indexed), `docs/command_reference/01-viewport.md`
  (new, indexed: `--snapshot`, its exit codes, recipes; T2 adds the keys),
  `docs/roadmap/README.md` current focus.
- `plan/README.md` now names `run.manifest.json`. The gate was failing on that line
  before this unit started (commit `c6c7710`). The manifest itself is untouched.

## Measured (development box, not the reference GPU)

Box: Quadro FX 3700 on nouveau (renderer `NV92`), Mesa 26.0.8, Qt 6.10.2, Ubuntu,
Release build. llvmpipe means `LIBGL_ALWAYS_SOFTWARE=1` (LLVM 21.1.8, 128 bits).

| Check | Result |
|---|---|
| Our `--snapshot` vs `dualc_field_view gyroid_box.json --snapshot` (pinned commit, built in a scratch tree outside the repo), NV92 | 198 / 480 000 px differ, max channel diff 2 |
| Same, llvmpipe | 246 / 480 000 px differ, all on silhouettes (hit/miss flips), max 198 |
| For scale: dualc_field_view NV92 vs llvmpipe | 31 878 px differ, max 78 |
| ctest `snapshot`, NV92 | passed, 0.9 s |
| ctest `snapshot`, llvmpipe | passed, 0.8 s; 460 colours, surface covers 30.8% of the frame |
| ctest `snapshot`, `env -u DISPLAY` | Skipped (exit 77, "cannot create an OpenGL context") |
| Live window, 1200×800, NV92 | renders the gyroid box on the same view (checked with an `xwd` capture) |

These are correctness checks, not performance measurements (E-04).

## What I got wrong on the way

- The first snapshot was upside down. `QOpenGLFramebufferObject::toImage(false)` keeps
  GL's bottom-up rows. The default `toImage()` (flipped = true) is right. I caught it
  because the background gradient is lighter at the top in the shader.

## Open

- **The `xvfb-run` path is untested.** xvfb is not installed on this box. Under
  xvfb-run `DISPLAY` is set, so the snapshot takes the normal xcb + GLX path that was
  tested here with hardware and llvmpipe. That should work, but it has not been run.
- **CI GL coverage.** On GitHub's ubuntu runner (no display) the snapshot test will skip.
  On windows-2022 and macos-14 it will either render or skip, depending on what GL the
  runner has. It has not run on CI yet: nothing is pushed.
- **A human should look at the window** (needsHumanCheck). The automated checks prove the
  snapshot matches DualC. They do not cover how the live window looks to a person, for
  example on HiDPI scaling (`devicePixelRatio`) or on the owner's GTX 1050 Ti.

## For the next unit (T2)

- `Viewport` holds `OrbitCamera camera_`. Mouse input should mutate it with the
  `raymarch_gl` constants (`orbitDragTo`: 0.008 rad/px, pitch clamped to ±1.5;
  `orbitDolly`: `dist *= 0.9^scroll`), then call `update()`. Pan does not exist in
  `OrbitState`. Its semantics are the T2 author's.
- `TraceRenderer::render(w, h, cam)` draws into whatever framebuffer is bound and calls
  `glViewport(0, 0, w, h)`. For the preview, bind a low-res FBO, render, then blit to
  `defaultFramebufferObject()`. Inside `QOpenGLWidget` the target is never FBO 0.
- `docs/command_reference/01-viewport.md` exists. Fill its "Keys and mouse" section and
  add the preview flag.
- For a reference image, `dualc_field_view` builds out of tree in about 10 minutes:
  `cmake -S build/_deps/dualc-src -B /tmp/ref -G Ninja -DDUALC_BUILD_EXAMPLES=ON
  -DDUALC_BUILD_FIELD_VIEW=ON -DDUALC_GC_DIR=$PWD/build/_deps/geometry-central-src` (it
  fetches GLFW).
