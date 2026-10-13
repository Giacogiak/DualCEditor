# Handoff — P07 T2: asynchronous shader compile (E-21)

Date: 2026-10-13. Plan: `plan/35_P07_render_scaling.md` § T2. DualC pin: `2fcb6ee` (unchanged).

**Status: done** on the automatic checks. The window's responsiveness on the GTX 1050 Ti
is a human check (below, and P07 T3).

## What changed and why

- **`src/trace_renderer.{h,cpp}`**. `setScene` is split into two halves.
  `linkProgram(gl, scene, …)` is static: it compiles and links on any current context and
  is timed as before. `adoptProgram(program, scene, ms)` swaps the program, grids, bindings
  and trace constants in one step. `countElsewhere` and `dropProgram` complete the API.
  `setScene` is `linkProgram` + `adoptProgram`, so its behaviour and counters are unchanged.
- **`src/viewport.{h,cpp}`**. `Viewport::buildScene(make, frame, finish)`:
  - A compile thread (`QThread::create`) owns a `QOpenGLContext` shared with the widget's,
    on a `QOffscreenSurface`. It runs `make` (DualC's codegen), then `linkProgram`, then
    `glFinish`, and queues the result to the GUI thread.
  - On the GUI thread, `finish` edits values, then `adoptProgram` runs and `sceneApplied`
    follows.
  - Tickets: every `buildScene`, `setScene` and `clearScene` supersedes the build in flight.
    The thread takes only the newest waiting build. Stale results are dropped and their
    programs deleted.
  - `releaseGL` stops the thread (it waits out a link under way) and restarts a build in
    flight on the next context.
  - New: `building()` and the `buildStarted` signal. `setScene` stays synchronous.
- **`src/main_window.{h,cpp}`**:
  - `apply` no longer lowers on the GUI thread. It passes `lowerScene` to `buildScene`, and
    the lowering comes back through a shared `Lowering`.
  - The carry-over of parameter values moved from `apply` to `carryValues`, which runs at the
    swap. It copies the values on screen, edits made during the compile included, and takes
    visible flags and colours from the pending doc.
  - A `Pending` that replaces another inherits its attempts (one `loadFinished` each), its
    `fresh` set and its `waits` (the files to watch if it fails).
  - Hide, show, recolour and the pick now work during a compile. Before, they were ignored
    while `pending_` was set, which was harmless when the compile blocked.
  - The status bar reads `compile …` while building.
- **`tests/async_compile_test.cpp`** (ctest `async_compile`) covers acceptance criteria 1–3
  (see the file header).
- **Docs**:
  - `docs/design/01-render-path.md` § Asynchronous compile closes E-21 on the design side.
  - E-21's *Built* line in `docs/decisions/03-deferred.md`.
  - `docs/command_reference/01-viewport.md` § While a scene compiles, and the status-bar row.
  - T2's notes in the plan.

## Acceptance criteria

1. **The swap while frames are drawn.** Every frame grabbed during a build (400 ms
   codegen sleep) equals the old scene's image. The first frame after the swap equals the
   new scene's image, drawn the same way through `setScene`. Exact image equality, 0 px.
2. **The last edit wins.** In the window: add a, then add b and add a again while the
   build is in flight → three objects on screen, 3 × `loadFinished(true)`. A hide made
   during the compile is kept. Also on a bare Viewport: of two builds in flight, one
   `sceneApplied`, the last one's image.
3. **The compile ms.** The status bar shows `compile C ms` again once the scene is on
   screen (`compile …` while building). `--bench` uses the synchronous `setScene`, so its
   `compileMs` is unchanged; the `bench` ctest passes.
4. **The command reference** says what the window shows during a compile.

## Measured (development box, 2026-10-13)

- `async_compile_test`, 320×240 viewport:
  - NV92: 5 frames and 14 timer ticks of 5 ms during a 400 ms build.
  - llvmpipe (`LIBGL_ALWAYS_SOFTWARE=1`): 3 frames and 6 ticks.
  - A gyroid frame grab takes about 80 ms on NV92, so few frames fit.
  - These are not performance figures (E-04). They only show that the GUI thread runs
    during a build.
- `python scripts/check.py` on NV92: 13/13, exit 0. The 12 earlier tests pass unchanged.

## What I got wrong on the way

- The first frame threshold (≥ 5 frames during the build) only just passed on NV92 and
  failed on llvmpipe's slower grabs. It is now ≥ 2 frames and ≥ 5 timer ticks.

## Open / human check

- **GTX, P07 T3:** add a third TPMS-in-box in the live window. The window should keep
  orbiting while it compiles, show `compile …`, then the compile ms. This box cannot judge
  it (E-04).
- **Not verified on another platform:** whether the driver links slower on a second
  context, or serialises the two contexts. The GTX run will show it in the status-bar
  compile ms against P01.4's cold figures.
- **The GTX `main_window` failure (§ P07.1)** may be a timing race: the test's `quiet()`
  waits for 500 ms with no paint, which a slow compile can outlast. The window path is now
  asynchronous, so a slow compile with no paint can still beat `quiet()`. The test was not
  edited.
- **Edge case, documented:** if an add fails at lowering after another add replaced it,
  both files go to the watch list, and each one is added only by its own next save.
- The reparent path (`initializeGL` with a scene on screen) still relinks synchronously.

## For the next unit

- P07.3 measures culling and compile on the GTX. The status-bar compile ms now comes from
  the compile thread, and `--bench compileMs` from the GUI thread, as before.
- P03 T3 (gizmo): a move stays a uniform upload (`Viewport::setParameter`) and works during
  a compile. A structural edit goes through `MainWindow::apply` → `buildScene`. Code that
  needs the new scene must wait for `sceneApplied` / `loadFinished`, not read it right
  after the call.
