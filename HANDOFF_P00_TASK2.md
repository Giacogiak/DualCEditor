# Handoff — P00 T2: orbit camera and adaptive preview

Date: 2026-10-10. Plan: `plan/10_P00_viewport.md` § T2. DualC pin: `2fcb6ee` (unchanged).

## What changed and why

- **Camera input** (`src/orbit_camera.{h,cpp}`). `orbitBy` and `dollyBy` are raymarch_gl's
  `orbitDragTo` and `orbitDolly`, with the same constants: 0.008 rad/px, pitch ±1.5,
  `dist *= 0.9^steps`, floor 1e-4. `panBy` is new, because `OrbitState` has no pan. It
  slides the target along the camera's right and up vectors, by
  `2·dist·tan(22.5°)/viewHeight` per pixel, so the point at the target stays under the
  cursor. `kTanHalfFov` now names the 0.41421356 the renderer used inline.
- **Viewport input** (`src/viewport.{h,cpp}`). Left-drag orbits. Middle-drag or Shift+left-drag
  pans. One wheel notch (`angleDelta().y()/120`) is one GLFW scroll unit. `F` frames the
  bounds: target and distance are reset, yaw and pitch kept. The key is matched on
  `QKeyEvent::text()`, case-insensitive, so it goes by the character typed, never by
  position (AZERTY rule). A drag ends when the button that began it comes up.
- **Adaptive preview.** `TraceRenderer::renderPreview` traces into a private RGBA8
  renderbuffer FBO at `int(fb × scale)`, then `glBlitFramebuffer`s it with `GL_LINEAR` into
  the widget's FBO. This is dualc_field_view's target and blit. The view counts as moving
  while a drag is held, and while a 180 ms `Qt::PreciseTimer` single-shot (`settle_`)
  runs. Every camera change and every button release restarts the timer. When it fires
  with no drag held and the last frame was a preview, the widget calls `update()` once
  for the full frame. Nothing repaints on a timer, so an idle widget runs no `paintGL`.
  The opening starts inside the settle window, so the first frame is a preview, as in
  dualc_field_view. The design page has the full rules and how they differ from DualC's.
- **`--preview-scale S`** (default 0.5, clamped to 0.1–1, 1 turns the preview off; a
  non-number exits 1). Same flag and default as dualc_field_view.
- **Paint log.** The `dce.viewport.paint` logging category prints one line per `paintGL`:
  kind, traced size, ms. While it is on, each frame ends with `glFinish`.
- **CMake.** Everything but `main.cpp` is now the static library `dualceditor_core`, so
  tests can drive the widget. The resources stay on the executable.
- **Tests.**
  - `orbit_camera` (GL-free): framing, the orbit and dolly constants, the pitch clamp and
    the dolly floor. It also checks that pan keeps the anchor under the cursor
    (re-projected), keeps the angles and distance, and that F after a pan restores the view.
  - `viewport` (preview 0.25) and `viewport_no_preview` (1.0) drive a real 800×600 `Viewport`
    with synthetic `QMouseEvent`, `QWheelEvent` and `QKeyEvent`. They check:
    - the first frame is a preview, and the opening settles full;
    - no paint in 1 s of idle;
    - 20 drag moves give 20 frames, all at the preview size;
    - a held, still drag paints nothing;
    - after release, exactly one full frame, no sooner than 180 ms and within 1 s of that;
    - no paint in 1 s of idle after the drag;
    - one wheel notch gives × 0.9;
    - both pan gestures move the target and keep the angles;
    - a key typing `d` at F's QWERTY scan code does not frame;
    - a key typing `f` with an unknown key code and another scan code frames, and Shift+F
      frames.

    Both report skipped (77) with no display, no GL 3.3 core context, or the
    `offscreen`/`minimal` platform.
- Docs: `docs/command_reference/01-viewport.md` (the flag, the mouse and key table,
  preview and settle, the paint log), `docs/design/01-render-path.md` (§ Adaptive preview,
  § Verified for T2), both README index lines, `docs/roadmap/README.md` current focus.

## Measured (development box, not the reference GPU)

Box: Quadro FX 3700 on nouveau (`NV92`), Mesa 26.0.8, Qt 6.10.2, Ubuntu, X11, Release
build, dpr 1. llvmpipe means `LIBGL_ALWAYS_SOFTWARE=1` (LLVM 21.1.8, 128 bits). The scene is
`gyroid_box.json`. Times come from `viewport_test`: the median over the 20 drag frames, and
the median over the full frames. They are paintGL wall time including `glFinish`.

| GL | Window | Preview | Drag frame | Full frame |
|---|---|---|---|---|
| llvmpipe | 800×600 | 0.25 (200×150) | 24–29 ms | 120–124 ms |
| llvmpipe | 1280×800 | 0.25 (320×200) | 21 ms | 184–220 ms |
| llvmpipe | 1280×800 | 0.5 (640×400) | 56 ms | 178 ms |
| llvmpipe | 800×600 | off (1.0) | 104 ms | 107 ms |
| NV92 | 800×600 | 0.25 | 15–16.5 ms | 176 ms |
| NV92 | 800×600 | off (1.0) | 92 ms | 96 ms |

So "orbiting stays responsive on llvmpipe at 0.25" is about 40–50 frames/s while dragging,
against about 5–9 frames/s at full resolution. These are not E-04 performance figures.

Real binary, NV92, `--preview-scale 0.25`, 1200×800 window, paint log on, untouched for
4 s: 3 preview frames (300×200), then 1 full frame (1200×800) at 0.41 s, then nothing.

## What I got wrong on the way

- **Redundant settle frame.** In my first version the settle timer always called
  `update()`. With the preview off, that painted an identical full frame 180 ms after
  every change. Press and release also repainted with an unchanged camera. Now the timer
  repaints only when the last frame was a preview, and press and release paint nothing.
  `viewport_no_preview` covers this.
- **The idle check after opening was flaky on llvmpipe.** The window manager resizes the
  window once it is mapped (a `Resize` and an `Expose` on the `QWidgetWindow` at about
  0.3–0.4 s). That repaints, and with slow frames it could land after the settle frame.
  This is an outside change, not idle repainting. The test now waits for the window to
  be exposed and for paints to stop before it starts the 1 s idle check.
- A scripted edit to the test file matched the wrong `runUntil` and cut out a block. I
  caught it at compile time and rebuilt the block. Nothing of it was committed.

## Open

- **A human should drive the window** (needsHumanCheck). The tests use synthetic events.
  Not covered:
  - how orbiting, panning and dolly feel with a real mouse and touchpad (touchpads send
    `angleDelta` in small fractions, so dolly steps are fractional);
  - `F` on the owner's physical Belgian AZERTY keyboard (F sits in the same place there
    as on QWERTY, so the scan-code test is synthetic);
  - HiDPI (pan uses logical pixels, the preview uses device pixels);
  - responsiveness on the GTX 1050 Ti (P00 T5).
- **Ctrl+F, Alt+F do not frame** (their `text()` is a control character or empty). That is
  deliberate, but no shortcut convention has been chosen yet.
- `viewport_test` on CI: it skips on the Ubuntu runner (no display). On windows-2022 and
  macos-14 it will run or skip depending on the runner's GL. It has not run there yet.

## For the next unit (T3)

- To swap scenes, `Viewport` needs a setter that calls `renderer_.setScene` under
  `makeCurrent()`. Keep the camera on a reload, as dualc_field_view does, and update
  `scene_.bounds`, which is what `F` frames. The file watch should call `update()` only on
  a real change. There is no idle timer to piggyback on, and the idle check in
  `viewport_test` must stay green.
- `viewport_test` takes `<graph> [scale] [w h]`, so it can be pointed at a heavier graph to
  get frame times.
- The paint log needs `QT_FORCE_STDERR_LOGGING=1` when stderr is piped. Otherwise Qt sends
  it to journald and it seems to vanish.
