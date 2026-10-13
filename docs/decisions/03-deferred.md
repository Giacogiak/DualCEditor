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
**Confirmed (Giacomo, 2026-10-13):** trigger now. Planned as P07 T2
(`../../plan/35_P07_render_scaling.md`).
**Built (P07.2, 2026-10-13):** structural edits lower, compile and link on a compile
thread with a context shared with the viewport's. The viewport draws the old program until
the new one links, and the last edit wins (`../design/01-render-path.md` § Asynchronous
compile). No editor program cache. The owner's responsiveness check is P07 T3.

## E-22 — Freeze an object to a baked grid
Bake a finished non-lattice object to a 3D texture so its per-step cost is constant.
Loses sharp edges and thin walls at 96³–256³, so lattices stay analytic. **Trigger:**
frame time with several mesh objects misses the target on the GTX 1050 Ti.
**Not fired (P02.1, 2026-10-12):** no scene of several mesh objects has been measured; one
mesh object settles at 7.3 GPU ms. `../roadmap/03-go-no-go.md` proposes the measurement for
P04 T2 (approved 2026-10-13, now in P04 T2's acceptance).

## E-23 — Per-object bounding-box culling in the shader

**Decided (Giacomo, 2026-10-12):** trigger on any clear slope in the P01 measurement; P02 plans the culling.
**Triggered (P01.4, 2026-10-12):** about +0.29 ms per added primitive and +2.5 ms per added
TPMS-in-box, settled GPU at 1280×800 on the GTX 1050 Ti (`../roadmap/02-measurements.md`).
**Trigger:** frame time grows with the number of non-overlapping objects (P01's measurement).
**Planned (P02.1, approved 2026-10-13):** P07 T1 (`../../plan/35_P07_render_scaling.md`).
**Built (P07.1, 2026-10-13):** each costly object's terms in `sceneSDF` and `sceneColor`
(TPMS, lattice, mesh, … — not analytic primitives; owner's option C) are skipped while
`p` is farther from its world box (`u_o<id>_boxMin`/`boxMax`, uniforms) than
the nearest object so far (`../design/03-scene-lowering.md` § Culling). Closes when P07 T3
measures the slope on the GTX.

## E-24 — Reuse of one object in several operations (DAG references)
DualC's graph is a tree (DualC D-12). **Trigger:** the scene model needs an object used by
two operations without copying it.

## E-25 — Undo and redo
**Trigger:** P03 (scene model).

## E-26 — Shadows and ambient occlusion
Not in scope (E-03). **Trigger:** the owner asks for them after P02.

## E-27 — Tiled export for large lattices in the export dialog
DualC's tiled STL export keeps peak memory at one tile. **Trigger:** P06.

## E-28 — An interruptible settle
The full-resolution frame is drawn in one draw on the GUI thread's context, so a drag that
starts while it is in flight waits for it: up to about 390 ms on the tapered octet
(`../roadmap/03-go-no-go.md`). Draw it in tiles over several frames, so a drag waits one
tile; optionally lower the preview scale on its own when a moving frame passes 33 ms.
Editor-side; the trace per pixel stays DualC's. Proposed by P02.1, approved by Giacomo on
2026-10-13. **Trigger:** P05 T2's octet fill still lags at the start of a drag on the
GTX 1050 Ti.
