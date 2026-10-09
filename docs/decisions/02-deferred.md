# Deferred and Dropped Decisions

## Deferred (will revisit)

### D-10: Async shader compilation
**Trigger:** Phase 1, when structural edits to the scene cause shader recompiles.

**Context:** Shader compile time on Intel HD 630 is multi-second (from DualC roadmap 12/04). For a scene with 5+ objects, each boolean or fill operation triggers a recompile = visible stall.

**Candidate fixes:** Async compile on a worker thread with the old shader kept live; program-binary cache; prune unused `sd*` helpers (all 36 always emitted); data-driven strut loop with uniform array instead of switch statements.

### D-11: Baked-mesh preview fidelity
**Trigger:** Phase 2 or later, when mesh import and booleans on meshes are common.

**Context:** The preview uses a ≤256³ baked grid; meshing uses the exact BVH. Visible preview/export mismatch for mesh-heavy scenes. Trade: full-res bake is slow and VRAM-heavy.

**Option:** Signal "preview resolution" badge to the user, or extend bake range for interactive resolution.

### D-12: DAG references in the field graph
**Trigger:** When a scene object is referenced or operated on twice.

**Context:** The graph is tree-only (no DAG refs, D-12 deferred in DualC). If object A is used in two booleans, it must be duplicated. Needed for "operate on object A twice" workflows.

### D-13: Undo/redo
**Trigger:** Phase 2 (scene model).

**Context:** Undo is a graph edit, not a viewport state. Snapshot-based (store full graph diff) vs incremental (log operations). Likely: per-operation snapshot to the undo stack.

### D-14: Lattice tiling for export
**Trigger:** Phase 4 (operations), when exporting large lattices is common.

**Context:** Depth 7 gyroid ≈ 5 GB monolithic; tiled is ≈ 62 MB with `--tile-depth 5`. Editor export dialog should expose tiling options (tile depth, which format).

### D-15: Mesh input sign method auto-selection
**Trigger:** Phase 3 (import), when dirty meshes are imported.

**Context:** `Diagnostics.inputWatertight` can drive an auto-switch to `winding` (GWN) if input is a soup. But `winding` is non-metric and needs `normalize` for smooth booleans. Guideline: offer both, default to best guess.

### D-16: Object freezing / bake cache
**Trigger:** Phase 5 (performance), when large scenes hit frame-time limits.

**Context:** "Freeze" non-lattice objects to a 3D texture (96³–256³ SDF grid); cost becomes constant per march step. Lattices stay analytic. Trade: loss of sharp edges and thin walls for meshes.

### D-17: Per-object bounding-box culling
**Trigger:** Phase 5, when scenes have many non-overlapping objects.

**Context:** Each object adds to the scene SDF cost. Early-out by bounding box per object can save evaluations. Needs codegen extension.

## Dropped (closed, not reconsidered)

None yet. All roadmap items from the plan are forward-looking or inherited from DualC.
