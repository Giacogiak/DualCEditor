# Settled Decisions

These choices are not re-litigated.

## D-01: Qt Widgets (not ImGui)
**Decision:** UI framework is Qt Widgets (Qt 5.15+ or Qt 6), with `QOpenGLWidget` for the 3D viewport.

**Rationale:** Rich docking system, property panels, tree views for scene objects. Qt was already integrated and then removed in DualC #50, but the removal was about Polyscope's depth, not Qt itself. The trade-off: heavier CI and packaging, but richer user experience.

**Constraint:** Qt 6 is LGPL; this app dynamically links only, so the library (DualC) is not affected. Per D-01 (DualC licensing), only code in the repo must be permissive. Dynamic Qt linking is outside that scope. If challenged, the editor can be MIT relicensed and Qt re-evaluated.

**Not deferred:** This is locked for Phase 0.

## D-02: Separate repo (not in-tree)
**Decision:** DualCEditor is a standalone repo, like Boletus (the .NET Rhino client).

**Rationale:** Clean boundary — DualC = engine + ABI, editor is a consuming client. Allows independent release cycles and dependency pinning. Fits the design principle D-20/D-22 (no side-car in DualC itself; the ABI and the library are all DualC ships).

**Implication:** DualCEditor pins DualC by git commit hash in CMakeLists.txt (FetchContent), so updates are explicit.

**Not deferred:** This is locked for Phase 0.

## D-03: Rendering scope (no shadows/AO initially)
**Decision:** Sphere tracing with key + fill light, no shadows, no ambient occlusion.

**Rationale:** Simpler to launch and measure. Shadows and SSAO can be added in a later phase if the RTX 1050 Ti baseline is headroom sufficient.

**Not deferred:** Locked for Phase 0; shadows/AO are deferred items for Phase 2+.

## D-04: Target GPU (RTX 1050 Ti 4GB)
**Decision:** Baseline is the user's laptop GPU (GeForce RTX 1050 Ti 4GB).

**Rationale:** Conservative anchor for performance targets. DualC docs already cite this GPU: full-res tapered octet at ~2s first frame, baked-mesh scenes seconds/frame. The editor must meet or exceed these numbers in preview mode.

**Not deferred:** Locked for Phase 0 spikes.

## D-05: Inherit DualC library scope rules
**Decision:** Mesh I/O, scene management, 3D transforms, and UI stay in the editor. DualC is pure geometry/mesh generation.

**Rationale:** Per D-04/D-20/D-22 of DualC. The library does not import meshes or manage scenes. The editor owns that responsibility.

**Not deferred:** This is a boundary condition, not deferred.
