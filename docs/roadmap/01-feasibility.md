# Feasibility analysis (2026-10-09)

The request was a standalone, fully raytraced editor over DualC's field graph. You drag
in meshes, see them as volumes, and chain operations: hard and smooth booleans, a
watertight solid from a mesh soup, a TPMS or strut lattice inside a volume, and booleans on
lattices. It has a panel to select, show and hide objects, a meshless file format, and a Qt
UI. File and line references are to DualC at the pinned commit.

## Verdict

The editor is feasible without rewriting the engine. DualC already computes almost
everything it needs. What is missing is the editor shell plus three seams: picking, edit
latency (shader recompiles), and meshes embedded in a saved scene.

## What DualC already provides

**Operation chains and their file form.** `examples/field_graph.{h,cpp}` holds a JSON tree
(`{version, units, root}`) and a `.fld` expression grammar. It covers:
- hard and smooth booleans (`k` defaults to 0.25)
- six TPMS fields
- strut lattices: sc, bcc, fcc and octet, tapered or graded, plus `mix`
- about 29 primitives
- transform and domain ops

Errors carry a JSON pointer. The op registries in `example_common` can drive generated
property panels.

**Rendering.** `examples/field_glsl.{h,cpp}` compiles a graph to GLSL with no GL dependency,
so it can run on a worker thread. The trace loop does 384 steps with a bisection refine,
key and fill lights, and section planes. A parameter edit is a uniform update, with no
recompile.

**Mesh soups.** The `winding` node (generalized winding number) turns a soup into a
watertight solid. `Diagnostics.inputWatertight` can choose between `mesh` and `winding` at
import.

**Meshless volumes.** The field graph *is* the meshless volume. `GridField` and
`bakeToGrid` are its baked form.

**Background work.** Fields are immutable and thread-safe. Extraction and export already
have `CancelToken` and `ProgressSink`, tiled streaming export, and atomic `.part` writes.

**Booleans on lattices** are where DualC is strongest. One field contour takes about 40 s;
the same operation done on meshes took 48.9 min.

## Blockers and corrections to the brief

1. **Every structural edit recompiles the scene shader.** Adding an object, a boolean or a
   fill means new GLSL, which takes seconds on weak GPUs (DualC D-29). Show and hide must be
   a uniform, not a recompile. Async compile and a program cache are E-21.
2. **The scene's per-pixel cost adds up.** One `sceneSDF` evaluates every object at every
   step. Two mitigations exist:
   - culling by bounding box (E-23);
   - freezing non-lattice objects to a baked grid (E-22). A grid of 96³–256³ loses sharp
     edges and thin walls, so lattices stay analytic.
3. **There is no picking.** `sceneSDF` returns a distance only (E-07). A CPU pick against the
   C++ field needs no engine change.
4. **A saved scene must embed its meshes.** Graph mesh nodes hold a path or an in-memory id,
   never the data. DualC's graph reader also ignores `version` (E-08).
5. **For meshes, the preview is not the result.** The preview samples a grid of at most
   256³; extraction uses the exact BVH. On a GTX 1050 Ti, baked-mesh scenes already take
   seconds per frame at full resolution (DualC 12/07).
6. **Smooth booleans fail on non-metric inputs.** `winding` and raw TPMS fields need
   `normalize` first (E-10). "Watertight at import" is best done in three steps:
   - contour the winding field;
   - clean the resulting mesh;
   - wrap it in an exact SDF node.

   This is slow on large soups, and the bake step cannot be cancelled (DualC D-45).
7. **The graph and codegen are example libraries, not the library.** They are linkable CMake
   targets, though, so the editor uses them as they are (E-06). The C ABI has no evaluation
   or GLSL entry points, so the editor links C++ directly.
8. **The graph is a tree.** An object used by two operations has to be copied (E-24).
9. **"Fully raytraced" means sphere tracing** with key and fill lights. Shadows and AO are
   extra marches per pixel (E-03, E-26).
10. **Meshing limits carry over.**
    - Depth 7 is the practical ceiling, and each extra level costs about 4× the faces and 8×
      the RAM.
    - TPMS fields never prune (`cellOverlaps` is always true). This is DualC #18, with about
      12× to gain at depth 7.
    - Large lattices export through tiles (E-27).

## Expected performance

These are estimates; the measured figures go in `02-measurements.md`.

**Recorded in DualC** (no fps figure exists anywhere):
- On a GTX 1050 Ti, a tapered octet reaches its first full-resolution frame in about 2 s,
  and baked-mesh scenes take seconds per frame at full resolution.
- On an Intel HD 630, the same octet is unusable at full resolution; the docs advise a
  preview scale of 0.35.

**Expected on the GTX 1050 Ti:** analytic scenes with a few objects and one lattice should
orbit interactively at a preview scale of about 0.35–0.5, then settle to full resolution
when idle. Mesh-backed objects and heavy tapered lattices will be the limit. The
development box (Quadro FX 3700) is good for UI work only.

**Extraction** (DualC roadmap 11):
- Depth 6 takes about 7 s.
- Depth 7 takes about 40–50 s and about 5 GB in one piece, or about 60 MB peak when tiled.
