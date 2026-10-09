# Phase 0: Spikes

Status: **IN PROGRESS** (2026-10-09)

## What we're testing

Two parallel spikes to validate the rendering architecture before committing to the full design:

### Spike A: Qt widget host
- Host the sphere-trace renderer in a `QOpenGLWidget` (not GLFW window)
- Adapt `raymarch_gl.cpp` from DualC examples/ (split windowing from rendering)
- Test: adapt a minimal field graph (e.g., a gyroid), render it in the Qt widget
- Measure: OpenGL state setup cost, Qt event-loop integration, preview scale settling

**Success criteria:**
- App launches, loads a .fld file or JSON graph, renders it interactively
- Camera orbits smoothly (left-drag)
- Preview scale adapts: low-res while dragging, full-res when idle (180 ms settle)
- No crashes on focus/resize

**Owner:** Spike A implementation in src/viewport.*

### Spike B: ID-returning codegen
- Extend `field_glsl.cpp` from DualC to emit a variant that returns `(distance, object_id)`
- Test: create a 3–5 object scene in the field graph (e.g., `union(obj1, obj2, ...)` with per-object tracking)
- Measure: frame time, overhead of id tracking, picking accuracy

**Success criteria:**
- Codegen produces a shader that returns distance and id
- No measurable regression in frame-time vs distance-only baseline
- Picking identifies the correct object by screen-space id
- Works on RTX 1050 Ti at target resolutions

**Owner:** Spike B in DualC's field_glsl.cpp (separate branch/PR to DualC)

## Next up

After spikes complete:
- **P1 Seams:** Promote graph lib, async compile, scene model
- **P2–P5:** Full editor shell, UI, operations

## Measurements (RTX 1050 Ti 4GB)

*To be filled in after spikes run:*

| Scene | Preview scale | FPS (settle) | Frame time (ms) | Notes |
|-------|---|---|---|---|
| TBD | | | | |

## Blockers

- Qt find_package needs Qt 5.15+ or Qt 6 installed on the test machine
- DualC must be fetched/built before linking (FetchContent in CMakeLists.txt)
- macOS may need special care for OpenGL 3.3 core (deprecated in newer versions)
