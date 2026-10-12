# P01 — Spike B: several objects, show/hide, picking

Waits on **E-07** (picking mechanism). This phase answers how the scene's cost grows with
the number of objects on the GTX 1050 Ti, and whether hiding and selecting can avoid a
shader recompile.

### T1. A scene of several objects in one shader

An editor-side `Scene` holds a list of objects. Each object has:
- a name
- a field graph
- a transform
- a visible flag
- a colour

The scene is lowered to a single graph, a union of each object wrapped in `transform`, and
compiled once. Visibility and colour are uniforms, not graph edits: a hidden object is
skipped, and showing it again does not recompile. Put the object id in each object's
uniform names, so the shader and the panel agree. Record the design in
`docs/design/03-scene-lowering.md`.

#### Acceptance criteria
- A scene of 3 objects renders, each in its own colour.
- Toggling an object's visibility neither recompiles nor relinks. Check by counting.
- `docs/design/03-scene-lowering.md` is written and indexed.

*Notes (2026-10-12, T1).* The plan first named `02-scene-lowering.md`; `02` was already
`02-graph-files.md` (P00 T3), and design pages number by birth order. The scene is one
shader compiled once, but each object's wrapped graph is compiled by DualC on its own and
renamed with its id, not one DualC graph: one graph loses the object of a hit (colour) and
DualC dedups equal objects into shared uniforms (`03-scene-lowering.md` § Why).

### T2. Objects panel

Add a dock listing the objects with a visibility checkbox, a colour swatch and a name.
Selecting a row selects the object, and Delete removes it. Dropping a second graph file
adds an object instead of replacing the scene.

#### Acceptance criteria
- Dropping two graph files gives two objects in the panel and in the viewport.
- Show, hide, select and delete all work from the panel.
- The command-reference page is updated.

### T3. Click to select

Implement picking as decided in E-07. The selected object is highlighted in the viewport
and in the panel, in both directions.

#### Acceptance criteria
- Clicking an object selects that object, and clicking empty space clears the selection.
  This holds where objects overlap on screen: the nearest object along the ray wins.
- A ctest covers picking against a known two-object scene, without GL when E-07 chose the
  CPU pick.

### T4. Frame time against object count — human check

The owner measures on the laptop, at preview scales 0.35 and 1.0, in a 1280×800 window.
Scenes:
- 1, 3, 5 and 10 analytic primitives
- 1 and 3 TPMS-in-box objects

#### Acceptance criteria
- The rows are in `docs/roadmap/02-measurements.md`.
- The handoff states the growth per added object, and whether E-23 (bounding-box culling)
  should be triggered.

## Exit criteria
- Tasks T1 to T4 are met. `python scripts/check.py` is green.
- `docs/roadmap/README.md` names P02.
