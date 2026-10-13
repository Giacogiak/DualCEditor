# Handoff — P07 T1: per-object bounding-box culling (E-23)

Date: 2026-10-13. Plan: `plan/35_P07_render_scaling.md` § T1. DualC pin: `2fcb6ee` (unchanged).

**Status: done** on the automatic checks. The acceptance criteria are met and the gate passes. A GTX re-run of the
cost-rule version is asked for (§ Human check asked).

History. The first session returned `blocked`: the development box's GL stack had wedged.
Giacomo rebooted it and answered on 2026-10-13 (§ The owner's answers). This second session
ran the gate and recorded the owner's GTX run, then returned two decisions. Giacomo answered
them (§ The owner's second answers), and a third session built the cost rule.

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

Gate after the reboot (development box, NV92): `python scripts/check.py`. The first run
failed one test, `viewport`, a timing-based test, on a box just rebooted. It passed when run
alone, and the second full run passed 12/12, exit 0. `scene_render` passes in 1.3 s on NV92.

## The owner's answers (Giacomo, 2026-10-13)

1. The box was rebooted. GL works again under llvmpipe and NV92.
2. The before/after `--snapshot` diff is a GTX human check, not llvmpipe or NV92.
3. The owner ran it, plus `measure_viewport.py --objects`, at dd92129. The bundle
   `.plan-run/P07_T1_GTX_2026-10-13.zip` is not tracked; its numbers are now in
   `docs/roadmap/02-measurements.md` § With per-object culling (P07 T1), and its raw JSON is
   `docs/roadmap/02-measurements-p07.jsonl`.

## Measured on the GTX 1050 Ti (owner, 2026-10-13, dd92129)

GL 3.3.0 NVIDIA 441.93, Qt 6.8.3, Windows 10, Release.
- `--snapshot` at 8b9baf8 against dd92129, every `--objects` scene (primitives,
  stacked in depth, TPMS-in-box): **0 / 480 000 px differ in each**. The acceptance
  tolerance was 16 px.
- Settled GPU per added object at 1.0, P01.4 (b011b24) → dd92129: primitives +0.28 →
  **+0.40 ms**, TPMS-in-box +2.49 → **+1.90 ms**, stacked in depth **+0.18 ms** (new).
- Cold compile at 0.35: 10 primitives 670 → **2009 ms**, 3 TPMS-in-box 1210 → **3503 ms**.
- The window with 3 TPMS-in-box: nothing missing, orbiting fine.
- ctest on the GTX: 11/12. `main_window` fails "the .fld renders a different image"
  (`tests/main_window_test.cpp:220`). The failure is the same at 8b9baf8, so it predates P07.

Reading: the culling is exact, but it does not pay for cheap primitives. The box test costs
about as much as a sphere's SDF. The first visible object in scene order is always
evaluated, so for 3 TPMS-in-box every ray pays object 1 as well as its own. The non-uniform
branches also triple the cold compile, which raises the stakes of P07 T2.

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

## The owner's second answers (Giacomo, 2026-10-13)

1. **P07.1 is accepted with the GTX `main_window` failure open.** That failure is its own item,
   to be investigated with the GTX in the loop; the GTX ctest stays red until then. P07.2 is
   not blocked by it. Recorded in `docs/roadmap/README.md` § Current focus. It predates P07,
   since 8b9baf8 fails `tests/main_window_test.cpp:220`, "the .fld renders a different image",
   on the GTX only.
2. **Culling option C: cull only costly objects.** Built in the third session:
   - `isCostlyObject(graph)` (`src/scene.{h,cpp}`) and `LoweredObject::culled`.
   - **Cheap, never culled:** a graph of at most 8 nodes (`kCheapObjectNodes`), all of them
     analytic primitives (`sphere`, `box`, `roundbox`, `ellipsoid`, `torus`, `capsule`,
     `cappedcylinder`), sharp booleans (`union`, `intersection`, `difference`) or
     placement (`translate`, `rotate`, `transform`, `scale`, `mirror`).
   - **Costly, culled:** anything else, such as a TPMS, a lattice, `mesh`, smooth booleans,
     `onion`, `offset`, `round`, `normalize`, warps, `repeat`, or more than 8 nodes.
     `docs/design/03-scene-lowering.md` § The cost rule.
   - A cheap object is written exactly as in P01, with no box uniform declared. A scene with
     no costly object omits `sceneBoxDist` too, so its shader is P01's byte for byte
     (`scene_test`). So the primitive families should return to P01.4's slope and compile
     times (the driver cache aside). The box bindings stay on every object, so
     indices, P03's move and the dock do not depend on the rule.
   - Tests. `scene_test` covers the rule, the source and the byte-for-byte cheap scene.
     `scene_render_test` step 4 now uses three thin shells (`onion(sphere)`, costly) for the
     box-upload and culled-vs-plain checks, and checks that spheres are not culled. Step 5
     stacks two shells for the pick-vs-image check, because spheres are no longer culled.

Measured after option C, development box, 400×300 (`scene_render_test`):
- llvmpipe: culled vs plain union 0 px for three spheres, three shells and gyroid_box;
  pick vs image 7500/7500 agree.
- NV92: culled vs plain union 0 px for the same three scenes; pick vs image 7486 agree,
  14 disagree (0.19 %, silhouette pixels; the bound is 0.5 %).
- On both, 4 links and 8 compiles before and after the box uploads.
- `python scripts/check.py`: 12/12, exit 0.

## Human check asked

On the GTX, at this unit's last commit:
`python scripts/measure_viewport.py --objects` and the `--snapshot` diff against 8b9baf8,
as in the owner's bundle. Expected:
- 0 px in every scene;
- the primitive slope back near +0.28 ms, and the primitive compile back near P01.4's
  (670 ms for 10, cold);
- TPMS-in-box near the dd92129 run (+1.90 ms per box).

The stacked-in-depth family is all primitives, so it is no longer culled. Its +0.18 ms
(dd92129) is now a cheap-object baseline. If a culled depth family is wanted, it needs
costly objects; T3 or the owner decides.

## Open

- The GTX `main_window` failure (above), its own item.
- NV92: it is still unknown whether the culled shader or the parallel GL runs hung it on
  the first session. Since the reboot, NV92 passes the gate with the culled shells.
- With costly objects, the first one in scene order is still always evaluated (§ Culling,
  Order). A two-pass scheme was offered as option B and not chosen.

## For the next unit

- P07.2 (async compile) is unaffected. Box bindings are ordinary bindings, so the
  program swap must upload them like any other.
- P03 T3 (gizmo): a move uploads `localFromWorld` **and** `worldBox(localBounds,
  worldFromLocal)` into `boxMinBinding`/`boxMaxBinding`. `MainWindow::parameterEdited`
  already does it for dock edits.
- P07.3: `measure_viewport.py --objects` now runs 3 families. It still labels rows `P01.4`;
  relabel them P07.3 by hand, or add a flag.
