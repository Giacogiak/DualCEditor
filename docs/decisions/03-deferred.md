# Deferred

Each item names the trigger that reopens it.

## E-20 — Promote DualC's graph and codegen into a supported library
Today they are DualC example libraries (E-06). **Trigger:** DualC changes their interface
under the editor, or a third client needs them.

## E-21 — Asynchronous shader compile and a program cache
A structural edit recompiles the whole scene shader, which takes seconds on weak GPUs
(DualC D-29). **Trigger:** P00's measurement shows a compile stall the owner finds
disruptive, or any phase from P03 onward.

**Triggered (P02.1, 2026-10-12), by the second clause:** P03 is the next phase. The
GTX 1050 Ti's cold compiles run 157–670 ms for 1–10 primitives (about +57 ms each) and
1210 ms for three TPMS-in-box, on the GUI thread; the driver's cache brings a source seen
before to 14–31 ms (`../roadmap/02-measurements.md`). Whether the stall is disruptive (the
first clause) is the owner's call. `../roadmap/03-go-no-go.md` plans it: asynchronous
compile first, an editor program cache only if measured to help.

## E-22 — Freeze an object to a baked grid
Bake a finished non-lattice object to a 3D texture so its per-step cost is constant.
Loses sharp edges and thin walls at 96³–256³, so lattices stay analytic. **Trigger:**
frame time with several mesh objects misses the target on the GTX 1050 Ti.
**Not fired (P02.1, 2026-10-12):** no scene of several mesh objects has been measured; one
mesh object settles at 7.3 GPU ms. `../roadmap/03-go-no-go.md` proposes the measurement for
P04 T2.

## E-23 — Per-object bounding-box culling in the shader

**Decided (Giacomo, 2026-10-12):** trigger on any clear slope in the P01 measurement; P02 plans the culling.
**Triggered (P01.4, 2026-10-12):** about +0.29 ms per added primitive and +2.5 ms per added
TPMS-in-box, settled GPU at 1280×800 on the GTX 1050 Ti (`../roadmap/02-measurements.md`).
**Trigger:** frame time grows with the number of non-overlapping objects (P01's measurement).
**Planned (P02.1, 2026-10-12):** `../roadmap/03-go-no-go.md` § Changes P03–P06 need, item 1.

## E-24 — Reuse of one object in several operations (DAG references)
DualC's graph is a tree (DualC D-12). **Trigger:** the scene model needs an object used by
two operations without copying it.

## E-25 — Undo and redo
**Trigger:** P03 (scene model).

## E-26 — Shadows and ambient occlusion
Not in scope (E-03). **Trigger:** the owner asks for them after P02.

## E-27 — Tiled export for large lattices in the export dialog
DualC's tiled STL export keeps peak memory at one tile. **Trigger:** P06.
