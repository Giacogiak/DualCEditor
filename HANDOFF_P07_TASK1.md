# Handoff — P07 T1: per-object bounding-box culling (E-23)

Date: 2026-10-13. Plan: `plan/35_P07_render_scaling.md` § T1. DualC pin: `2fcb6ee` (unchanged).

**Status: blocked. The code is written; verification is incomplete.** The llvmpipe results
below pass. Then the development box's GL stack wedged: the NV92 GPU hung, and after that
even `LIBGL_ALWAYS_SOFTWARE=1 QT_QPA_PLATFORM=offscreen DualCEditor --snapshot` of one
sphere hangs at 0 % CPU. The box needs a reboot. Until then the full `scripts/check.py`
(its GL ctests) cannot run; `check.py --fast` passes.

## What changed and why

- **`src/scene.{h,cpp}`**. `lowerScene(scene, cull = true)`. Each object gets two more
  bindings: `u_o<id>_boxMin` and `u_o<id>_boxMax` (vec3, keys `boxMin`/`boxMax`, pointer
  `/objects/<id>`), its world box. `LoweredObject::boxMinBinding` and `boxMaxBinding` give
  their indices. Each term of `sceneSDF` and `sceneColor` becomes
  `if (visible) { float b = sceneBoxDist(p, boxMin, boxMax); if (b <= max(d, 0.0)) ... }`.
  `cull = false` writes P01's plain union, byte for byte; it is the tests' reference.
  `worldBox(local, worldFromLocal)` (was the private `transformBox`) is now public.
- **`src/main_window.cpp`**. A `matrix` edit in the Parameters dock uploads the moved
  culling box, as uniforms. The dock shows `boxMin`/`boxMax` greyed out.
- **`scripts/measure_viewport.py --objects`** gains the family `N stacked in depth`
  (N = 1, 3, 5, 10). The same spheres and boxes sit in a column along the default view axis,
  1.2 apart, the first nearest the camera. Its growth line is printed after the primitives'.
- **`tests/scene_render_test.cpp`** gains steps 4–5:
  - a culling-box upload neither compiles nor links, and the culling reads the boxes;
  - the culled images equal `lowerScene(scene, false)`'s (three spheres; gyroid_box as one
    object);
  - in two spheres stacked in depth, the CPU pick and the culled image agree pixel by pixel.
- **Docs**: `docs/design/03-scene-lowering.md` § Culling (new; it closes E-23 on the design
  side), the command reference's Parameters section, and E-23's *Built* line in
  `docs/decisions/03-deferred.md`.

## Measured (development box, llvmpipe, before the box wedged)

`scene_render_test` with `LIBGL_ALWAYS_SOFTWARE=1`, 400×300, passed:
- culled vs plain union: three spheres 0 px differ; one-object gyroid_box 0 px;
- one-object scene vs the graph alone: 0 px (unchanged from P01);
- 3 links, 6 compiles before and after the toggles and the box uploads;
- pick vs culled image, 7500 sampled px: 7500 agree, 0 disagree (near 356, far 109,
  none 7035).

**Not done: the `--snapshot` before/after of the six P01.4 scenes.** Reference PNGs from
the pre-change binary (8b9baf8) were made on NV92 and llvmpipe. The after-run was made with
the clipped first version (see below), then the box wedged before a clean run of the final
binary. Owner: compare `--snapshot` of the `--objects` scenes at 8b9baf8 and at this commit.

## What I got wrong on the way

- **The first version clipped each evaluated field to its box**, `max(o_sdf, b)`. On
  llvmpipe it changed gyroid_box's image (2875 px, up to 76 levels), because that graph's own
  field is below the box distance near the box's edges. Removed: the culling only skips.
- **The first box-upload test moved object 1's box.** The first object is never culled,
  because nothing is nearer yet. The test now moves the last object's box.
- **I ran NV92 snapshots and a GL ctest in parallel.** The culled 10-object shader on NV92
  ran for minutes. A `scene_render_test` on NV92 then waited on the GPU at 0 % CPU, and was
  killed. After that, no GL program starts on this box, software or not.
- `pkill -f` with a pattern that matched my own shell killed it twice.

## Facts found

- **NV92 (nouveau) already draws scenes of 5 or more objects wrong** at 8b9baf8, before
  this change: 480 000 / 480 000 px differ from llvmpipe for 5 and 10 primitives and for 3
  TPMS-in-box. The development box cannot judge many-object images; llvmpipe can.
- llvmpipe snapshots of 1 and 3 objects took about a second each before the change.

## Open — for Giacomo

1. **Reboot the development box**, then run `python scripts/check.py`. If it hangs on
   `scene_render` with the NV92, run that test once with `LIBGL_ALWAYS_SOFTWARE=1` to tell
   a driver hang from a bug.
2. **GTX check (human):** `--snapshot` of the `--objects` scenes (P01.4 families plus
   stacked-in-depth) at 8b9baf8 and at this commit should match within driver noise. Open
   the window with 3 TPMS-in-box: nothing missing, and the frame time no worse.
3. Whether the divergent per-pixel branch the culling adds is what hung NV92 is not known.
   The GTX run in T3 settles whether it costs more than it saves.

## For the next unit

- P07.2 (async compile) is unaffected. Box bindings are ordinary bindings, so the
  program swap must upload them like any other.
- P03 T3 (gizmo): a move uploads `localFromWorld` **and** `worldBox(localBounds,
  worldFromLocal)` into `boxMinBinding`/`boxMaxBinding`. `MainWindow::parameterEdited`
  already does it for dock edits.
- P07.3: `measure_viewport.py --objects` now runs 3 families. It still labels rows `P01.4`;
  relabel them P07.3 by hand, or add a flag.
