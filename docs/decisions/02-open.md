# Open decisions

Each one blocks the phase named in `README.md`'s table. When the owner answers, add
`**Decided (<owner>, <date>):** <answer>` at the top of its section and mark the table row.

## E-07 — Picking mechanism

**Decided (Giacomo, 2026-10-12):** CPU pick (the plan's recommendation).

A click must select the object under the cursor. Today's generated `sceneSDF` returns a
distance only (DualC `examples/field_glsl.cpp`).

- **CPU pick (recommended).** On click, march the one camera ray on the CPU through each
  visible object's C++ field (`ImplicitField::valueAt`), and take the nearest hit. Needs no
  DualC change, costs one ray per click, and is exact. The selection highlight is then a
  per-object tint uniform, added when the scene composes objects.
- **GPU id buffer.** Codegen returns `(distance, objectId)` and the shader writes the id to
  a second target. Allows hover highlight at no extra cost per frame. Needs a DualC PR to
  `field_glsl.cpp` and its parity harness.
- **Both**: CPU pick now, GPU ids later if hover highlighting is wanted.

## E-08 — Scene file format (`.dcscene`)

DualC's graph references a mesh by path or by an in-memory id and never embeds one, so a
saved scene must carry its meshes itself.

- **Zip container (recommended).** `scene.json` (objects, transforms, visibility, each
  object's field graph) plus `meshes/<n>.bin` and optional `grids/<n>.bin` baked SDFs.
  miniz is already vendored by DualC (MIT). Inspectable with any unzip tool.
- **Single JSON file**, meshes base64-encoded. Simplest, but about 1.33× larger and slow to
  parse for big meshes.
- **Custom binary.** Fastest, but opaque and needs its own versioning.

Whichever is chosen, the reader must check `version` (DualC's graph reader ignores it).

## E-09 — Mesh import formats and readers

geometry-central (already linked through DualC) reads OBJ and PLY. STL and 3MF are what
CAD and print users drag in.

- **OBJ, PLY and STL first (recommended).** A small own STL reader (binary and ASCII, with
  vertex welding), MIT. 3MF is deferred.
- **All four now.** 3MF via miniz plus a small XML parse of the mesh part.
- **OBJ and PLY only**, through geometry-central. Users convert STL elsewhere.

## E-10 — Smooth booleans on non-metric inputs

`winding` (for mesh soups) and raw TPMS fields are not distance fields, and DualC's smooth
booleans misbehave on them unless wrapped in `normalize`.

- **Wrap automatically (recommended).** The editor inserts `normalize` under any non-metric
  operand of a smooth boolean, and shows it in the object's history.
- **Warn and let the user choose.**
- **Forbid**: smooth booleans are disabled on non-metric operands.
