# Roadmap

## Current focus

**Now: P07, render scaling (`../../plan/35_P07_render_scaling.md`).** Per-object
bounding-box culling (E-23), asynchronous shader compile (E-21), then a GTX re-measurement.
Added by P02 and run before P03, since neither waits on E-08.
T1 is done: costly objects (TPMS, lattices, meshes, …) skip their SDF while `p` is farther
from their world box than the nearest object, and cheap analytic objects are written as in
P01 (`../design/03-scene-lowering.md` § Culling). The owner's GTX run of a first version,
which culled every object, is in `02-measurements.md` § With per-object culling (P07 T1).

**Known failure, its own item (owner, 2026-10-13):** on the GTX 1050 Ti, `main_window`
fails one check, "the .fld renders a different image" (`tests/main_window_test.cpp:220`),
at 8b9baf8 and at P07.1, so before P07. It passes on the development box. To be
investigated with the GTX in the loop. Until then the GTX ctest is 11/12. It does not
block P07.

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

P07 (above), then P03, which waits on E-08 (`../decisions/02-open.md`).

## Files

- `01-feasibility.md` — the 2026-10-09 analysis: what DualC already provides, the blockers, the expected performance.
- `02-measurements.md` — frame and compile times measured on the reference GPU (E-04), filled by P00 and P01.
- `02-measurements-p00.jsonl` — the raw `--bench` output of the owner's two P00.5 runs on the GTX 1050 Ti (2026-10-10).
- `02-measurements-p01.jsonl` — the raw `--bench` output of the owner's P01.4 run on the GTX 1050 Ti (2026-10-12).
- `02-measurements-p07.jsonl` — the raw `--bench` output of the owner's run at P07.1's commit dd92129 (culling) on the GTX 1050 Ti (2026-10-13).
- `03-go-no-go.md` — P02's verdict from the measurements: what met the feasibility target, what missed, the deferred items triggered, and the plan edits proposed for P03–P06.
