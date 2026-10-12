# Deferred

Each item names the trigger that reopens it.

## E-20 — Promote DualC's graph and codegen into a supported library
Today they are DualC example libraries (E-06). **Trigger:** DualC changes their interface
under the editor, or a third client needs them.

## E-21 — Asynchronous shader compile and a program cache
A structural edit recompiles the whole scene shader, which takes seconds on weak GPUs
(DualC D-29). **Trigger:** P00's measurement shows a compile stall the owner finds
disruptive, or any phase from P03 onward.

## E-22 — Freeze an object to a baked grid
Bake a finished non-lattice object to a 3D texture so its per-step cost is constant.
Loses sharp edges and thin walls at 96³–256³, so lattices stay analytic. **Trigger:**
frame time with several mesh objects misses the target on the GTX 1050 Ti.

## E-23 — Per-object bounding-box culling in the shader

**Decided (Giacomo, 2026-10-12):** trigger on any clear slope in the P01 measurement; P02 plans the culling.
**Trigger:** frame time grows with the number of non-overlapping objects (P01's measurement).

## E-24 — Reuse of one object in several operations (DAG references)
DualC's graph is a tree (DualC D-12). **Trigger:** the scene model needs an object used by
two operations without copying it.

## E-25 — Undo and redo
**Trigger:** P03 (scene model).

## E-26 — Shadows and ambient occlusion
Not in scope (E-03). **Trigger:** the owner asks for them after P02.

## E-27 — Tiled export for large lattices in the export dialog
DualC's tiled STL export keeps peak memory at one tile. **Trigger:** P06.
