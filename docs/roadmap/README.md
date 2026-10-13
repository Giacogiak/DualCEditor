# Roadmap

## Current focus

**The plan is done (2026-10-13).** Every phase in `../../plan/README.md` (P00 to P07) has
run, and P06 was the last. No phase is next until the owner adds one. Still open: the GTX
checks queued for the owner's next laptop session (P03, P04, P05, and P06's export timings),
gathered as one checklist in `04-gtx-batch-2026-10-13.md`; E-28's verdict
(`../decisions/03-deferred.md`); the cold-compile question and the `main_window` failure
on the GTX, both below and both in that checklist's last section.

Done: P06, export (`../../plan/70_P06_export.md`, `../../HANDOFF_P06_TASK1.md`).
`File ▸ Export Selected Object…` (`Ctrl+E`) and `Export Visible Scene…` (`Ctrl+Shift+E`)
open a dialog: the file, whose extension picks STL, 3MF or OBJ; the depth, with an
estimate of the triangles and the RAM; the bounds; and DualC's tiled STL and 3MF, which
closes E-27. DualC writes on a worker thread, with progress and a Cancel that leaves no
file (`../design/08-export.md`). A gyroid fill at depth 6 writes a watertight file, and the
estimate is 0.89× the real count there.

Done: P05, operations (`../../plan/60_P05_operations.md`). T1
(`../../HANDOFF_P05_TASK1.md`): `Operations ▸ Union … Smooth Difference` on the objects
Ctrl+clicked in the Objects dock, in that order, folded left over more than two, with a
`Blend k` in mm; non-metric operands of a smooth boolean go under `normalize` (E-10). T2
(`../../HANDOFF_P05_TASK2.md`): `Operations ▸ Fill with TPMS…` and
`Fill with Lattice…` fill the selected object, with an optional skin; its GTX check (the
octet fill against the tapered octet, E-28's trigger) is queued. T3
(`../../HANDOFF_P05_TASK3.md`): the menu entries and fill panels are generated from DualC's
registries (`operationCatalogue`, `OperationPanel`), and the command reference lists the
operations.

Done: P04, import (`../../plan/50_P04_import.md`). E-09 is decided (OBJ, PLY and STL;
3MF deferred). T1, the mesh readers (`../design/07-mesh-import.md`,
`../../HANDOFF_P04_TASK1.md`); T2, import as a solid (`../../HANDOFF_P04_TASK2.md`): a
dropped mesh becomes a `mesh` object when DualC finds it watertight and it is oriented,
else a `winding` object; `Edit ▸ Make Watertight` contours a winding object on a worker
thread, with progress and Cancel; `.dcscene` carries imported meshes. Its GTX check (E-22's
trigger, `--meshes`) is queued.

Done: P03, scene model (`../../plan/40_P03_scene_model.md`). T1, operation history and
undo (`../design/04-scene-model.md`, `../../HANDOFF_P03_TASK1.md`); T2, `.dcscene`
(`../design/05-dcscene-format.md`, `../../HANDOFF_P03_TASK2.md`); T3, the transform gizmo
(`../design/06-transform-gizmo.md`, `../../HANDOFF_P03_TASK3.md`). Its GTX checks are
queued.

**Open from P07 (`../../HANDOFF_P07_TASK3.md`):** cold shader compiles are longer than
P01.4's, and the reason is not known:
- 10 primitives take 1137 ms against 670, although primitives keep P01's shader;
- 3 TPMS-in-box take 1897 ms against 1210;
- the window's compile thread takes 2567 ms for the same scene.

The window stays responsive through them.

**Known failure, its own item (owner, 2026-10-13):** on the GTX 1050 Ti, `main_window`
fails one check, "the .fld renders a different image" (`tests/main_window_test.cpp:220`),
at 8b9baf8 and at P07.1, so before P07. It passes on the development box. To be
investigated with the GTX in the loop. Until then the GTX ctest is one short (12/13 at 4b89008). It does not
block P07, and is still open after it.

Done: P07, render scaling (`../../plan/35_P07_render_scaling.md`), added by P02 and run
before P03.
- T1: costly objects (TPMS, lattices, meshes, …) skip their SDF while `p` is farther from
  their world box than the nearest object. Cheap analytic objects are written as in P01
  (`../design/03-scene-lowering.md` § Culling).
- T2: a structural edit lowers, compiles and links on a compile thread, and the viewport
  draws the old image until the new program links (`../design/01-render-path.md`
  § Asynchronous compile).
- T3: the owner re-measured on the GTX at b163141 with a cold shader cache
  (`02-measurements.md` § GTX 1050 Ti at b163141). Per added object, settled GPU at 1.0:
  - TPMS-in-box +1.96 ms, against +2.49 without culling;
  - primitives +0.29 ms, against +0.28;
  - a column in depth +0.13 ms.
- The window kept orbiting through a cold 2.6 s compile.
- E-21 and E-23 are closed. P07 is closed.

Done: P02, go or no-go (`../../plan/30_P02_go_no_go.md`). `03-go-no-go.md` says **go**,
approved by the owner on 2026-10-13. Every analytic and one-mesh scene measured orbits at
0.35–0.5 well inside 16.7 ms and settles in under 10 ms; the tapered octet misses
(81–128 ms moving against a 33 ms bar), as the feasibility note expected, and
`dualc_field_view` is no faster on it. E-23 and E-21 are triggered and planned in P07; E-22
is not measured (P04 T2 now measures it); E-28 (interruptible settle) is new, behind P05
T2's octet check.

Done: P01, scene and picking, Spike B (`../../plan/20_P01_scene_and_picking.md`). E-07
is decided (CPU pick). T1 is done: a `Scene` of objects lowers to one shader, each object
in its own colour, and visibility and colour are uniforms (`../design/03-scene-lowering.md`).
`--snapshot` and `--bench` take several graphs. T2 is done: the window holds a scene, a
drop adds an object, and the Objects dock shows, hides, selects and deletes objects. T3 is
done: a click in the viewport selects the nearest object along the ray (a CPU pick), and the
selection is highlighted in the viewport and the dock. The owner checked it on the laptop.
T4 is done: the owner measured scenes of 1 to 10 objects on the GTX 1050 Ti with
`scripts/measure_viewport.py --objects` (`02-measurements.md` § Against object count). Each
added primitive costs about +0.29 ms and each added TPMS-in-box about +2.5 ms of settled
GPU time at 1280×800. The worst scene, 3 TPMS-in-box, stays under 9.4 ms. E-23 is
triggered. P01 is closed.

Done: P00, the viewport, Spike A (`../../plan/10_P00_viewport.md`). T1 is done: the viewport
sphere-traces `gyroid_box.json` through DualC's codegen, and `--snapshot` matches
`dualc_field_view`. T2 is done: the orbit camera (orbit, dolly, pan, `F` to frame) and
the adaptive preview, which settles after 180 ms and does not repaint while idle. T3 is
done: a graph opens by `File ▸ Open`, by argument or by a drop. It reloads when saved, and
a broken file shows its error and keeps the last image. T4 is done: a Parameters dock edits
the graph's bindings as uniforms only, with no relink, and the status bar shows the
shader compile ms and the GPU ms of the last settled frame. T5 is done: the owner measured
on the GTX 1050 Ti with `--bench` and `scripts/measure_viewport.py` (`02-measurements.md`). The image matches dualc_field_view, and
the moving frame time is within about 10% on the like-for-like rows. P00 is closed.

## Next up

P03, once E-08 is decided (`../decisions/02-open.md`).

## Files

- `01-feasibility.md` — the 2026-10-09 analysis: what DualC already provides, the blockers, the expected performance.
- `02-measurements.md` — frame and compile times measured on the reference GPU (E-04), filled by P00 and P01.
- `02-measurements-p00.jsonl` — the raw `--bench` output of the owner's two P00.5 runs on the GTX 1050 Ti (2026-10-10).
- `02-measurements-p01.jsonl` — the raw `--bench` output of the owner's P01.4 run on the GTX 1050 Ti (2026-10-12).
- `02-measurements-p07.jsonl` — the raw `--bench` output of the owner's GTX 1050 Ti runs of P07.1 (2026-10-13): the first 20 lines at dd92129 (culling all objects), the next 20 at 6f3a8bd (culling costly objects only, option C); and of P07.3: the last 20 at b163141, cold shader cache.
- `03-go-no-go.md` — P02's verdict from the measurements: what met the feasibility target, what missed, the deferred items triggered, and the plan edits proposed for P03–P06.
- `04-gtx-batch-2026-10-13.md` — the one laptop session that clears the GTX checks queued by P03–P06 (and two older items): what to run at the newest commit, what is expected, where the answers are filed.
