# Questions for the DualC owner

DualC is not edited from here (rule 5): the editor links DualC's host-side libraries at the
pinned commit `2fcb6ee` (E-06), and a DualC change the editor would like is a question for
the owner and a PR on DualC. This page gathers every such question the plan produced,
with the editor-side workaround in place and what the change would remove. **Status
(2026-10-14): none is recorded as raised in writing, and no DualC PR exists for any.** A
question that is answered gets a dated line here; a change that lands in DualC is taken
up at the next pin bump.

## Q1 — `emitMesh` and `emitWinding` should resolve `id=` (P04 T1)

`FieldGraph::build` accepts `mesh(id=…)` and `winding(id=…)` through
`MeshResolver::resolveId`, but the GLSL codegen at the pin (`examples/field_glsl.cpp`,
`emitMesh`, `emitWinding`) refuses them: "'mesh' requires a 'path'". Workaround:
`lowerScene` hands the codegen a copy of the graph with each id written as the path
`dualceditor-mesh-id:<id>` (`meshIdsAsPaths`, `kMeshIdPath`), and
`GraphDirMeshResolver::resolve` routes that path back to `resolveId`
(`../design/07-mesh-import.md`). Cost: one graph copy per lowered object, nothing
measurable. The change would remove the adapter.

## Q2 — A parameter registry on the operations (P05 T3)

`tpmsKinds()`, `strutKinds()` and the booleans carry a name and a blurb but no parameter
list; the builder reads params by name in `field_graph.cpp` (`k`, `wavelength`,
`thickness`, `radius`, `nodeRadius`, and `k`'s default `numParam(n, "k", 0.25)`). The
editor mirrors them per family in `src/operation_catalogue.cpp`; its `operations` ctest
raises each of the 27 parameters and checks DualC's field changes, so a rename in DualC
fails the test, but the editor does not follow on its own, and a new family or a new
parameter needs an editor change. A `params` field on `TpmsKind`, `StrutKind` and the
booleans would remove the lists.

## Q3 — `Diagnostics*` on the tiled writers (P06 T1)

`writeFieldTiledStl` and `writeFieldTiled3mf` take no `Diagnostics*`, so a tiled export
logs "tiled, tile depth D" instead of a triangle count and watertightness
(`../design/08-export.md`).

## Q4 — Expose the RAM estimate (P06 T1)

`kBytesPerSurfaceFace` is file-local in DualC and `chooseTileDepthForBudget` only picks a
depth and prints its estimate. The editor mirrors the constant as `kExportBytesPerFace`
(3 KiB per face). A public estimate would remove the mirror.

## Q5 — Per-node feature scale for host-side edits (P00 T4, P03 T3)

`stepScale` and `featureScale` come from the compiled graph (`nodeFeatureScale` is not
exported). A parameter edit that shrinks a wavelength far below the file's, or a large
uniform scale-down of an object, can step past thin features until the next compile
(`../design/03-scene-lowering.md` § Edits that do not compile). An editor-side fix for the
scale exists without DualC (scale `featureScale` by the smallest object scale in
`pushUniforms`); the parameter case needs DualC's per-node rule.

## Q6 — Applying a binding to a `GraphNode` (P01 T3)

The CPU pick builds each object's C++ field from the file's values, so parameter edits
other than the pose are not seen by a click; writing a uniform back into a node needs
per-op knowledge of DualC's flat `params` packing. P06's `exportGraph` later did write
edits into params on the editor side (`src/mesh_export`), which the pick could reuse; a
DualC helper would be the one home for that knowledge.

## Q7 — DualC's OBJ reader fails on a file with a comment and an `o` line (P05 T2)

A `.fld` naming `tests/data/meshes/cube.obj` fails in DualC's mesh reader with
`basic_string::substr`. The file has a comment line and an `o` line, which the editor's
own reader accepts. Not investigated further (rule 5); a bare OBJ was used instead. Worth
a bug report with that file.

## Q8 — A colour hook in the trace framework (P01 T1)

`assembleTraceShader`'s framework has one fixed colour line,
`vec3  base = vec3(0.80, 0.82, 0.86);`. The editor replaces it by string match with
`sceneColor(hp)` and throws if the line is not there exactly once, so a pin bump that
changes it fails `scene_render_test` (`../design/03-scene-lowering.md`). A hook (a
`#define` or a function the framework calls) would make the patch unnecessary.

## Q9 — A GPU id buffer, if hover highlighting is ever wanted (E-07)

The option not taken in E-07: codegen returns `(distance, objectId)` and the shader writes
the id to a second target, which allows hover highlight at no extra cost per frame. It
needs a DualC PR to `field_glsl.cpp` and its parity harness. The CPU pick stands until
hover is asked for (`../decisions/02-open.md` E-07).

---
← Back to the [Roadmap index](README.md).
