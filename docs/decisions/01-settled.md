# Settled decisions

Decided by the owner on 2026-10-09, in the feasibility session (`../roadmap/01-feasibility.md`).

## E-01 — Qt 6 Widgets for the UI

Qt 6 (6.4 or newer) Widgets, with a `QOpenGLWidget` GL 3.3 core viewport. Docks, a scene
tree and property panels are what Qt Widgets does well; QML is not used. Qt 5 is not
supported (end of life). Dear ImGui (MIT, lighter) was the alternative considered.

Licence: Qt is LGPL and is linked dynamically, never vendored. DualC D-01 ("LGPL never")
governs code copied into a repository, which this is not. The editor itself is MIT.

## E-02 — A separate repository

The editor is its own repository, a client of DualC like Boletus: DualC stays engine and
ABI (DualC D-20, D-22), and the editor has its own release cycle. DualC is pinned by
commit in `CMakeLists.txt`.

## E-03 — Lighting: key and fill only

The viewport keeps DualC's sphere-tracer shading: a key light plus a fill light, with no
shadows and no ambient occlusion. Shadows and AO are deferred (E-26).

## E-04 — Reference GPU: GeForce GTX 1050 Ti 4 GB (the owner's laptop)

Performance targets and measurements use this card, which is the same one DualC's preview
notes cite (DualC roadmap 12/04). The development box (Quadro FX 3700, nouveau) is not a
measurement machine. Measurement units are human checks run on the laptop.

## E-05 — DualC's library scope holds

Mesh I/O, scene management, transforms and UI live in the editor; DualC receives fields
and meshes in one global frame (DualC D-04).

## E-06 — Consume DualC's host-side libraries by pin

The editor links DualC's `dualc_examples_fieldglsl`, which brings `dualc_examples_fieldgraph`,
`dualc_examples_common` and `dualc::dualc`, by configuring DualC with `DUALC_BUILD_EXAMPLES=ON`.
No DualC change is needed to start. Promoting these into a supported library is deferred
(E-20).
