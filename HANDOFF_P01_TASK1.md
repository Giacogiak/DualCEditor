# Handoff — P01 T1: a scene of several objects in one shader

Date: 2026-10-12. Plan: `plan/20_P01_scene_and_picking.md` § T1. DualC pin: `2fcb6ee` (unchanged).

**Status: done** on the automatic checks. A human look is asked for (below): the colours
were checked by pixel read-back on this box, not by eye on the GTX 1050 Ti.

## What changed and why

- **`src/scene.{h,cpp}`** (new). `Scene` / `SceneObject` (id, name, graph, meshDir,
  rigid `transform`, `visible`, `colour`), `sceneColourFor`, `objectUniformName`,
  `lowerScene` → `LoweredScene` (one `GlslScene` + per-object binding indices and boxes),
  and `assembleSceneShader`. Design: `docs/design/03-scene-lowering.md`.
- **Lowering, in short.** Each object's graph is wrapped in DualC's `transform` node,
  compiled by `compileToGlsl` on its own in its local padded box, then every identifier
  gets the object id (`f7` → `o2_f7`, `u_n3_radius` → `u_o2_n3_radius`, `sceneSDF` →
  `o2_sdf`). The editor appends `u_o<id>_visible` / `u_o<id>_colour`, a `sceneSDF` that is
  the min over visible objects (uniform branch), and a `sceneColor` that returns the
  colour of the nearest visible object. The trace box is the union of every object's moved
  box, hidden ones included.
- **Colour patch.** DualC's framework has one fixed colour line,
  `vec3  base = vec3(0.80, 0.82, 0.86);`. `assembleSceneShader` replaces it with
  `sceneColor(hp)` only for a lowered scene, and throws if the line is not there exactly
  once (a pin bump that changes it fails `scene_render_test` loudly). A single graph's
  shader stays DualC's byte for byte.
- **`TraceRenderer`**: links `assembleSceneShader(scene)`; new `compileCount()` (also on
  `Viewport`) beside `linkCount()`.
- **`graph_scene` / `graph_file`**: `parseGraphText`, `traceBounds`, `collectNodeOps`
  split out of `compileGraphText` (same behaviour); `GraphDirMeshResolver` and
  `graphMeshDir` made public; `parseGraphFile` (parse only).
- **`main.cpp`**: `--snapshot` and `--bench` take several `graph` arguments, one object
  each. The window still refuses a second graph (exit 1, message now says "the window").
- **Tests** (10 ctests now): `scene` (GL-free: namespacing, unique names, object
  uniforms and values, localFromWorld matrix, union box incl. hidden, stepScale and
  featureScale, the patch, unpatched single graph, errors name the object, ids not
  reused) and `scene_render` (GL off-screen, skips 77).
- **Docs**: `docs/design/03-scene-lowering.md` (new, indexed), command reference §
  *Several graphs* and the `graph` row, roadmap current focus, plan T1 notes.

## Measured (development box, not the reference GPU)

Quadro FX 3700, nouveau NV92, Mesa 26.0.8; and llvmpipe (`LIBGL_ALWAYS_SOFTWARE=1`,
LLVM 21.1.8). `scene_render_test` at 400×300:
- one-object scene of `gyroid_box.json` vs the graph alone: **16 / 120 000 px differ, by
  1 level (NV92); 0 px (llvmpipe)**;
- three spheres coloured pure-ish red, green, blue read back as mean (204, 75, 75),
  (76, 207, 76), (77, 77, 208) on both drivers;
- **toggle counting**: after 3 hides, 3 shows and 1 recolour, links 3 → 3, shader
  compiles 6 → 6. Showing again restores the image pixel for pixel.

No frame times: those are T4, on the laptop (E-04).

## What I got wrong on the way

- I first asserted the one-object scene is bit-identical to the graph alone. On NV92 it
  is not (16 px by 1 level): the program differs, so the driver's codegen does. The test
  now allows 0.1% of pixels and prints the count and the worst difference.
- The plan's page number `02-scene-lowering.md` was already taken by
  `02-graph-files.md`. The page is `03-scene-lowering.md`; the plan is corrected.

## Deviation from the plan, settled here

The plan says "lowered to a single graph … compiled once". It is one shader compiled and
linked once, but not one DualC graph: a single `compileToGlsl` gives no object-of-hit for
colour, and DualC's DAG dedup would merge two equal objects into one function and one
uniform set. Each object is compiled by DualC separately and renamed. No DualC change.
Reasons in `03-scene-lowering.md` § Why.

## Open

- **Rigid transforms only** (DualC's `transform` inverts with `inverseRigid`). A scale
  needs a different lowering (or `scale` node) later.
- **A transform edit as a uniform** must upload `localFromWorld`; the trace box is not
  updated until the next `lowerScene`, so a moved object can leave the box.
- With every object hidden, each pixel marches the whole box (no hit). Cheap enough, but
  T4 may want an early-out.
- `sceneColor` re-evaluates every visible object once per hit pixel; T4's growth figure
  will include it.

## For the next unit (T2, Objects panel)

- The window still loads one graph through `loadGraphFile` → `Viewport::setScene`. T2
  should hold a `Scene` in `MainWindow`, lower it on each structural change and call
  `Viewport::setScene(lowered.scene, frame)`; show/hide/recolour go through
  `Viewport::setParameter(lowered.objects[i].visibleBinding, {0|1})`.
- The Parameters dock lists every binding: it will show each object's transform mat4,
  `visible` and `colour` unless filtered (their `nodePointer` is `/objects/<id>`). Graph
  bindings of different objects can share a `nodePointer` (each graph's own pointers), so
  group them by `LoweredObject::firstBinding..endBinding`, not by pointer alone.
- Keep object 1's default colour DualC's grey: then a one-graph window still matches
  `dualc_field_view`.

## Human check asked

Run `./build/bin/DualCEditor --snapshot three.png a.fld b.fld c.fld` with the three files
of `docs/command_reference/01-viewport.md` § Several graphs, on the laptop: a grey sphere,
an orange box and a blue sphere, all lit like the single-graph view. Optionally
`--bench` with the same three files, to see the multi-graph bench run on the GTX.
