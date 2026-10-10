# Roadmap

## Current focus

P00: the viewport, Spike A (`../../plan/10_P00_viewport.md`). T1 is done: the viewport
sphere-traces `gyroid_box.json` through DualC's codegen, and `--snapshot` matches
`dualc_field_view`. T2 is done: the orbit camera (orbit, dolly, pan, `F` to frame) and
the adaptive preview, which settles after 180 ms and does not repaint while idle. T3 is
done: a graph opens by `File ▸ Open`, by argument or by a drop. It reloads when saved, and
a broken file shows its error and keeps the last image. T4 is done: a Parameters dock edits
the graph's bindings as uniforms only, with no relink, and the status bar shows the
shader compile ms and the GPU ms of the last settled frame. T5 is next: the owner measures
on the GTX 1050 Ti.

## Next up

P01, scene and picking (Spike B). It waits on E-07. P02 then decides go or no-go from the
GTX 1050 Ti measurements.

## Files

- `01-feasibility.md` — the 2026-10-09 analysis: what DualC already provides, the blockers, the expected performance.
- `02-measurements.md` — frame and compile times measured on the reference GPU (E-04), filled by P00 and P01.
