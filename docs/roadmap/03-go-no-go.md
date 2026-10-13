# Go or no-go (P02, 2026-10-12)

The measured answer to `01-feasibility.md`. Its target, § Expected performance: on the
GTX 1050 Ti, *analytic scenes with a few objects and one lattice should orbit
interactively at a preview scale of about 0.35–0.5, then settle to full resolution when
idle. Mesh-backed objects and heavy tapered lattices will be the limit.*

Every number below is a cell of `02-measurements.md`. A row is cited as
*unit · scene · preview scale* of its first table (for example **P00.5 · gyroid_box.json ·
0.35**), or by its section for the other tables. All rows: GTX 1050 Ti, Windows 10, driver
441.93, traced 1280×800, medians of 60 frames, vsync off, unless the row says otherwise.

The feasibility note gives no frame time for "interactive". This note reads it as
**at most 33 ms per moving frame (30 fps)**, and 16.7 ms (60 Hz) as smooth. The owner's
live impressions (P00 T5 checklist, step 6) are quoted where they exist.

**Status: approved (Giacomo, 2026-10-13).** The owner approved the verdict, kept 33 ms as
the interactive bar, confirmed E-21's trigger now, chose option 1a (a new phase P07), and
approved items 2–5. All are applied: `../../plan/35_P07_render_scaling.md`, P03 T3, P04 T2
and P05 T2's acceptance criteria, and E-28 in `../decisions/03-deferred.md`.

## Verdict: go

The editor proceeds as planned. Every analytic and baked-mesh scene measured orbits at
0.35–0.5 well inside 16.7 ms and settles to full resolution in under 10 ms, with up to ten
objects. The one miss is the scene the feasibility note named as the limit, a heavy tapered
lattice, and DualC's own viewer is no faster on it. Two deferred items fire (E-21, E-23);
both are editor-side work and need no DualC change (rule 5).

## What was met

- **Light analytic scene, one object.** gyroid_box.json moves at 0.8 GPU / 3.8 wall ms at
  0.35 and 1.1 / 3.8 at 0.5, and settles at 2.6–2.7 GPU / 4.1–5.3 wall ms
  (**P00.5 · gyroid_box.json · 0.35, 0.5, 1**). The owner: "immediate".
- **A baked mesh with a lattice.** mesh_lattice.json moves at 1.9 / 3.9 ms (0.35) and
  2.7 / 3.8 ms (0.5), and settles at 7.3 GPU / 8.2–8.3 wall ms
  (**P00.5 · mesh_lattice.json · 0.35, 0.5, 1**). The owner: "immediate". This is better
  than the feasibility note feared from DualC's record ("seconds per frame" for baked-mesh
  scenes): one mesh object at 1280×800 is a 7 ms frame.
- **A few objects.** Ten non-overlapping primitives settle at 3.2 GPU / 4.1 wall ms and
  three TPMS-in-box at 7.6 / 8.6 ms (**P01.4 · 10 primitives · 1**, **P01.4 · 3 TPMS-in-box ·
  1**). The moving wall time at 0.35 is 2.8–4.1 ms for all six object-count scenes
  (**P01.4 · \* · 0.35**).
- **A crisp settle.** On every scene but the tapered octet, the settled full-resolution
  frame (2.6–8.6 wall ms) is under 16.7 ms, so the settle is one frame after the 180 ms
  idle wait.
- **Parity with DualC.** The image matches `dualc_field_view` (§ Against dualc_field_view,
  snapshot table: at most 715 of 480 000 px, the owner saw no difference). The moving
  frame time is within about 10% on the like-for-like 0.35 rows (+10%, +6%, −2%).

## What was missed, and by how much

- **The tapered octet is not interactive.** It moves at 80.6 GPU / 83.3 wall ms at 0.35 and
  127.6 / 128.6 ms at 0.5 (**P00.5 · tapered octet (P12) · 0.35, 0.5**): 2.5× and 3.9× the
  33 ms bar, 12 and 8 fps. Its settled frame is 375–390 GPU ms (**P00.5 · tapered octet
  (P12) · 0.35, 0.5, 1**), so the settle comes about 0.56 s after the drag ends (180 ms
  wait plus one frame). The owner: it lags, "mostly at the start of a drag".
  - *Not the editor's cost.* `dualc_field_view` takes 73.0 ms per drag frame at 0.35 on
    the same scene (§ Against dualc_field_view). The cost is the trace of a non-metric
    tapered lattice, DualC's shader.
  - *The start-of-drag lag is the settle frame.* The full-resolution frame is drawn on the
    GUI thread's GL context in one draw. A drag that starts while it is in flight waits
    for it, up to about 390 ms. This is a reading of the render path
    (`../design/01-render-path.md`), not a measurement.
  - *Predicted.* `01-feasibility.md` names heavy tapered lattices as the limit. P05 T2's
    "Fill with lattice" will produce scenes like this one.
- **The fixed per-frame cost.** At 1.0 the editor's moving frame is +15% (+0.6 ms, gyroid)
  and +15% (+1.2 ms, mesh_lattice) slower than `dualc_field_view` (§ Against
  dualc_field_view), against a 10% bar. The wall time does not fall below about 2.6–4 ms
  even when the trace takes 0.2 ms (**P01.4 · 1 primitive · 0.35**: 0.2 GPU / 4.1 wall).
  At 250–380 fps this costs nothing against 16.7 ms. No change proposed.
- **The cold compile grows with the scene.** First compiles: 157 / 265 / 413 / 670 ms for
  1 / 3 / 5 / 10 primitives, 339 / 1210 ms for 1 / 3 TPMS-in-box (**P01.4 · \* · 0.35**,
  shader compile column), and 945.4 ms for mesh_lattice.json (P00.5 conditions, under the
  first table). From the driver's cache the same sources take 14–31 ms
  (**P01.4 · \* · 1**, **P00.5 · \***). The cache only helps a source it has seen: every
  structural edit (add an object, an operation) writes new GLSL, so it pays the cold time.
  The compile and link run on the GUI thread, so the window freezes for that long: 1.2 s
  for a third TPMS box, and by the +57 ms per primitive slope several seconds for the
  scenes P05 builds. No target was set for it; the feasibility note expected "seconds on
  weak GPUs".
- **Frame time grows with the object count.** About +0.29 ms per added primitive and
  +2.5 ms per added TPMS-in-box, settled GPU at 1.0 (§ Against object count, growth table:
  +0.28 and +2.49). Within budget at the counts measured, but linear with no culling.

## Not measured

- **Several mesh objects.** Every P01.4 scene is analytic. E-22's trigger cannot be judged.
- **Objects stacked in depth.** The P01.4 primitives lie in z = 0, so most rays meet one or
  two objects.
- **Status-bar settled `frame`.** It reads 7–12× above the bench on light scenes (P00 T5
  findings). The bench is the measurement; the status bar stays diagnostic. Open.
- **Extraction.** Export times are DualC's (`01-feasibility.md` § Extraction), not
  re-measured; P06 owns them.

## Deferred items the numbers trigger

| Item | Trigger | Fired? | Evidence |
|---|---|---|---|
| E-21 Async compile and program cache | a compile stall the owner finds disruptive, **or any phase from P03 onward** | **Yes**, by its second clause: P03 is the next phase. The owner confirmed it on 2026-10-13. | Cold compiles of 0.16–1.2 s on the GUI thread (above). |
| E-22 Freeze an object to a baked grid | frame time with several mesh objects misses the target | **No**: not measured. One mesh object settles at 7.3 GPU ms. | **P00.5 · mesh_lattice.json · 1** |
| E-23 Bounding-box culling | frame time grows with the number of non-overlapping objects | **Yes**, at P01.4 (recorded then). | § Against object count, growth table |

Recorded on each item in `../decisions/03-deferred.md`.

**E-21, what to build.** The driver's disk cache already turns a source seen before into a
14–31 ms compile (above), so on the GTX an editor-side program cache would mainly help an
undo back to an earlier scene. The part the numbers ask for is the asynchronous compile:
generate the GLSL off the GUI thread (DualC's codegen has no GL dependency), compile and link
in a context shared with the viewport's, and keep drawing the old program until the new one
links.

**E-23, what to build.** `lowerScene` writes `sceneSDF` and `sceneColor` itself
(`../design/03-scene-lowering.md`), and has every object's world box. Each visible object's
term becomes "skip if the distance to its box is already larger than `d`": that distance is a
lower bound of the object's, so the union is unchanged. The box goes in as a uniform pair, as
the visibility flag does, so a transform edit (P03 T3) moves it without a recompile. No
DualC change.

## Changes P03–P06 need

Proposed by P02.1; all approved by the owner on 2026-10-13 and applied.

1. **Schedule E-23 and E-21 before P05.** P05 is where object counts and structural edits
   multiply. Two ways:
   - **(a, recommended, chosen)** a new phase, `plan/35_P07_render_scaling.md` (P07, the next free
     phase id), that runs right after P02. Neither task depends on E-08, so it does not wait
     on P03's open decision. Needs new units in `plan/run.manifest.json` (the owner's).
   - **(b)** two new tasks in P03, T4 (E-23) and T5 (E-21), after the gizmo. No new phase,
     but they wait on E-08 with the rest of P03.

   Tasks, either way:
   - *Culling (E-23).* Acceptance: `--snapshot` of the P01.4 scenes is unchanged against
     before; `scene_render_test` still counts no compile on hide, show and recolour; the
     owner re-runs `measure_viewport.py --objects` on the GTX and the per-primitive slope
     falls well below +0.28 ms; a scene family stacked in depth is added to `--objects`.
   - *Async compile (E-21).* Acceptance: adding a third TPMS-in-box keeps the window
     responsive and drawing the old scene until the new one links; a ctest swaps a
     program without a frame of a half-built scene; the owner times the freeze on the GTX.
2. **P03 T3 (gizmo).** The trace box `uBoundsMin` / `uBoundsMax` and, once E-23 lands, each
   object's culling box must follow a move as uniforms, or a moved object leaves the box
   (`../design/03-scene-lowering.md` § Edits that do not compile). Add to its acceptance:
   a moved object is still drawn whole, with no compile.
3. **P04 T2 (import).** Add a human check: frame time with 1, 3 and 5 imported mesh
   objects on the GTX, through a `--meshes` family in `measure_viewport.py`. It is E-22's
   trigger, which nothing measures today.
4. **P05 T2 (fills).** Add a human check: the octet fill of an imported mesh, timed on the
   GTX at 0.35, against **P00.5 · tapered octet (P12) · 0.35**. Its result decides
   item 5.
5. **A new deferred item, E-28 (recorded): an interruptible settle.** Draw the
   full-resolution frame in tiles over several frames, so a drag that starts during it
   waits for one tile, not the whole frame; optionally lower the preview scale on its own
   when a moving frame passes 33 ms. **Trigger:** P05 T2's octet fill still lags at the
   start of a drag. Editor-side; the trace per pixel stays DualC's.
6. **P06.** No change from these numbers.
