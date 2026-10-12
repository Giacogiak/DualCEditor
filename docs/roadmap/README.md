# Roadmap

## Current focus

**Now: P01, scene and picking (Spike B, `../../plan/20_P01_scene_and_picking.md`).** E-07
is decided (CPU pick). T1 is done: a `Scene` of objects lowers to one shader, each object
in its own colour, and visibility and colour are uniforms (`../design/03-scene-lowering.md`).
`--snapshot` and `--bench` take several graphs. T2 is done: the window holds a scene, a
drop adds an object, and the Objects dock shows, hides, selects and deletes objects. Next: T3,
click to select.

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

P01 (above). P02 then decides go or no-go from the GTX 1050 Ti measurements.

## Files

- `01-feasibility.md` — the 2026-10-09 analysis: what DualC already provides, the blockers, the expected performance.
- `02-measurements.md` — frame and compile times measured on the reference GPU (E-04), filled by P00 and P01.
- `02-measurements-p00.jsonl` — the raw `--bench` output of the owner's two P00.5 runs on the GTX 1050 Ti (2026-10-10).
