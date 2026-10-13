# 03 — Scene lowering

How a scene of several objects becomes one shader. Born in P01 T1 (2026-10-12).

## The scene

`src/scene.{h,cpp}`. A `Scene` holds a list of `SceneObject`s:

| Field | Meaning |
|---|---|
| `id` | Given by `Scene::add`, counting from 1. Permanent: a removed id is never reused. It names the object's uniforms. |
| `name` | For messages and, from T2, the panel. |
| `graph` | The parsed field graph (`GraphNode`). |
| `meshDir` | The folder relative mesh paths resolve against first (`GraphDirMeshResolver`), as for a graph opened alone. |
| `transform` | `worldFromLocal`, a `dualc::Mat4`. **Rigid only**: DualC's `transform` node inverts it with `Mat4::inverseRigid`, so a scale would be wrong. |
| `visible` | Uploaded as a float uniform, 1 or 0. |
| `colour` | Three floats in the space of the trace shader's `base`, that is linear, before the framework's 1/2.2 gamma. |

`sceneColourFor(id)` gives the default colour: DualC's own grey (`kDualCBase`,
0.80, 0.82, 0.86) for object 1, so a scene of one graph looks as the graph alone does, then
a fixed palette of seven (orange, blue, green, yellow, purple, teal, pink), cycled.

## Lowering

`lowerScene(scene)` returns a `LoweredScene`: one `GlslScene`, which `TraceRenderer` links
as it links a single graph, and one `LoweredObject` per object, giving where its uniforms
sit in `bindings`.

For each object:

1. `traceBounds(graph)`: the graph's own padded trace box, in the object's local frame
   (the same function a graph opened alone uses).
2. The graph is wrapped in DualC's `transform` node (`matrix` = the object's transform)
   and compiled **on its own** by `compileToGlsl`, with the local box as `bounds`. Meshes
   under the transform are in local coordinates, so they bake in the local box.
3. Every identifier the codegen made gets the object id (`Namespacer`):

   | DualC | Object 2 |
   |---|---|
   | `f7` | `o2_f7` |
   | `u_n3_radius`, `u_n5_tex`, `u_n5_texMin` | `u_o2_n3_radius`, `u_o2_n5_tex`, `u_o2_n5_texMin` |
   | `sceneSDF` | `o2_sdf` |

   The same rename is applied to the binding names and the mesh sampler names, so the
   renderer's binding upload and `uploadGrid3D` (which derives `_texMin` etc. from the
   sampler name) work unchanged.
4. Two uniforms the object owns are added, with bindings whose `nodePointer` is
   `/objects/<id>`: `u_o<id>_visible` (float) and `u_o<id>_colour` (vec3).
   `objectUniformName(id, key)` builds these names, so the panel (T2) and the shader agree.

5. Two more, for the culling (§ Culling, P07 T1): `u_o<id>_boxMin` and `u_o<id>_boxMax`
   (vec3 each, keys `boxMin` and `boxMax`), the object's world box (`worldBounds`).

Then the generated source ends with the union and the colour lookup (shown here
without the culling, as `lowerScene(scene, false)` writes it; § Culling gives the default):

```glsl
float sceneSDF(vec3 p) {
  float d = 1e10;
  if (u_o1_visible > 0.5) d = min(d, o1_sdf(p));
  if (u_o2_visible > 0.5) d = min(d, o2_sdf(p));
  return d;
}
vec3 sceneColor(vec3 p) {   // the colour of the nearest visible object
  float d = 1e10;
  float id = 0.0;
  vec3 c = vec3(0.8, 0.82, 0.86);
  if (u_o1_visible > 0.5) { float di = o1_sdf(p); if (di < d) { d = di; c = u_o1_colour; id = 1.0; } }
  ...
  if (u_selected > 0.5)       // the selection highlight (P01 T3)
    c = abs(id - u_selected) < 0.5 ? mix(c, vec3(1, 0.78, 0.2), 0.45) : c * 0.45;
  return c;
}
```

The branch is on a uniform, so all fragments take the same path: a hidden object's field
is not evaluated. With every object hidden, `sceneSDF` is 1e10 everywhere and the trace
reaches the end of the box with no hit (background), at the cost of a full march.

Scene-level values:

| Value | From |
|---|---|
| `bounds` | Union of every object's local box moved by its transform (8 corners, axis-aligned). Hidden objects count, so hiding moves neither the trace box nor the framing. |
| `stepScale` | Minimum over the objects. One TPMS object makes the whole scene non-metric (`uMetricSDF` 0). |
| `featureScale` | Minimum of the objects' non-zero ones; 0 when none has one. |
| `nodeCount` | Sum. |
| `meshes` | Concatenated; mesh *i* takes texture unit *i*. |

An object that fails to build or compile throws `GraphError` with
`object <id> (<name>): ` before DualC's message, and DualC's pointer. An empty scene throws.

## Culling

P07 T1, closes E-23. `lowerScene(scene)` (`cull` true, the default) writes each term of
`sceneSDF` and `sceneColor` behind a test on the object's world box:

```glsl
float sceneBoxDist(vec3 p, vec3 lo, vec3 hi) { return length(max(max(lo - p, p - hi), 0.0)); }
float sceneSDF(vec3 p) {
  float d = 1e10;
  if (u_o1_visible > 0.5) { float b = sceneBoxDist(p, u_o1_boxMin, u_o1_boxMax); if (b <= max(d, 0.0)) d = min(d, o1_sdf(p)); }
  ...
}
```

and the same test before `float di = o<id>_sdf(p)` in `sceneColor`.

- **Why the min is unchanged.** An object's surface lies inside its padded trace box, and
  the world box contains the moved trace box. Outside the box, a true SDF is at least the
  distance to the box, so when that distance `b` is already larger than the nearest `d`, the
  object cannot lower `d`. The term is skipped. `max(d, 0)` keeps an object evaluated
  while `p` is inside its box (`b` = 0), even when `d` is already negative.
- **Non-metric fields** (TPMS) are not bounded below by the box distance. Skipping one is
  still safe for the trace: the value returned is at most `b`, so a step cannot enter the
  skipped object's box, where all of its surface is. Only the step length can change, not
  the surface found. The TPMS-in-box scenes measured unchanged on llvmpipe
  (§ Verified (P07 T1)).
- **No clip.** A first version also clipped each evaluated field to its box,
  `max(o_sdf(p), b)`. It changed the gyroid_box image (2875 px, up to 76 levels, on
  llvmpipe): that graph's own field is below the box distance near the box's edges. The
  clip was dropped. An object whose surface leaves its own box is not supported either way;
  the pick clips each object to its own box (§ Picking), the culling only skips it.
- **Order.** Objects are tested in scene order. The first visible object is always
  evaluated (`d` is 1e10). A near object listed first culls the most.
- **The box is a binding**, `nodePointer` `/objects/<id>`, as the visible flag is. A move
  uploads `worldBox(localBounds, worldFromLocal)` (`src/scene.h`) and compiles nothing
  (`scene_render_test`). The window does this on a `matrix` edit in the Parameters dock
  (`MainWindow::parameterEdited`). The dock shows `boxMin` and `boxMax` greyed out. A
  re-lowering keeps them with the object's other uploaded values, so they stay with the
  kept transform. A box moved off its object is a bug: the object vanishes where others
  are nearer.
- **Hidden objects** still skip on `u_o<id>_visible` first, as before.
- **The scene trace box** (`bounds`) is unchanged: still the union, hidden objects included.

`lowerScene(scene, false)` writes P01's plain union, byte for byte. It exists so the
tests can compare the culled image with the plain one. The box bindings are emitted in both
modes, so binding indices do not depend on it; unused, they upload to location −1.

## Why each object compiles on its own

The plan asked for the scene "lowered to a single graph, a union of each object wrapped in
`transform`, and compiled once". It is one shader, compiled and linked once, but not one
DualC graph:

- **Colour needs the object of a hit.** One `compileToGlsl` call returns `sceneSDF` and
  nothing that maps a function back to the object it came from.
- **DualC dedups equal subtrees.** Two objects with the same graph and the same
  transform would share one function and one uniform set. An edit meant for one would
  move both.
- **Visibility is not an op.** DualC has no node that a uniform can switch off.

Compiling each wrapped object separately, then renaming, keeps DualC's codegen untouched
(rule 5) and gives each object its own functions and uniforms. The cost is a regex pass
over the generated source per object (a few kB, well under a millisecond against a
compile of tens of ms).

## The colour patch

DualC's trace framework (`kTraceFramework` in `examples/field_glsl.cpp`) sets a fixed
surface colour on one line, `vec3  base = vec3(0.80, 0.82, 0.86);`. `assembleSceneShader`
takes `assembleTraceShader`'s output and, **only when the generated source defines
`sceneColor`**, replaces that line with `vec3  base = sceneColor(hp);`. The rest of the
framework (march, normals, key and fill light, section-plane tint) is DualC's, unchanged.
A single graph is not patched, so its shader is byte for byte DualC's (`scene_test`).

If a DualC pin bump changes that line, `assembleSceneShader` throws ("DualC's trace
framework changed…"), `TraceRenderer::setScene` fails with that message, and
`scene_render_test` fails. The alternative, copying the whole framework into the editor,
was not taken: it would drift from DualC silently.

## Edits that do not compile

`TraceRenderer::setParameter(binding)` uploads one uniform. Visibility, colour and the
transform matrix are bindings like any graph parameter, so showing, hiding, recolouring
and (later) moving an object neither compile nor link. `TraceRenderer::compileCount` and
`linkCount` count `glCompileShader` and `glLinkProgram`. `scene_render_test` checks that
both stay put across three hides, three shows and a recolour.

The transform binding holds DualC's `localFromWorld`, row-major
(`worldFromLocal.inverseRigid()`), not the object's `transform`. A move written as a
uniform edit must upload the inverse, and the object's culling box (§ Culling). The trace
box stays as lowered until the next `lowerScene`, so a moved object can leave it.

## Where it is used

- `--snapshot` and `--bench` with several `graph` arguments (`src/main.cpp`,
  `../command_reference/01-viewport.md` § Several graphs).
- The window (P01 T2, `src/main_window.cpp`). It holds a `Scene` and lowers it again on
  each structural change (open, add, delete, a reloaded file); show, hide and a colour edit
  set a uniform. A lowering keeps the current values of the bindings of every object whose
  graph did not change (matched by the object's binding range and `paramKey`), so adding
  or deleting an object does not undo parameter edits. The Parameters dock gets each
  binding's `nodePointer` with `/objects/<id>` in front, since two graphs share pointers.

## Picking and the selection highlight

P01 T3, E-07 (CPU pick). `src/pick.{h,cpp}`, no GL.

- **The ray.** `viewRay(camera, x, y, width, height)` is the trace framework's `rd` for
  `vUV = (2x/width − 1, 1 − 2y/height)`, from `uCamPos`. Widget pixels or framebuffer
  pixels give the same ray, since only the ratios count.
- **The fields.** `ScenePicker(scene, lowered)` builds each object's own graph (not the
  `transform`-wrapped one) with `FieldGraph::build`, the C++ field the exporter uses,
  in its local frame, with its own `GraphDirMeshResolver` (a mesh builds its BVH here).
  `MainWindow` builds it at the first click after a commit and drops it on the next commit.
- **The march.** The ray goes to the object's frame through `localFromWorld` (rigid, so
  `t` is the same in both frames), is clipped to the object's local trace box, and is
  marched as the shader marches: steps of `stepScale · 0.8 · f / |∇f|` (`|∇f|` = 1 when the
  object's `stepScale` is 1, else central differences with `h = stepMin`), clamped to
  `[stepMin, stepMax]` computed as `TraceRenderer` does but from the object's box and
  `featureScale` (`LoweredObject::stepScale`, `featureScale`), then 20 bisections on the
  first sign change. No step cap of 384: a pick is one ray.
- **The nearest wins.** Each visible object gives its first crossing; the smallest `t`
  is the pick. `sceneSDF` is the min of the visible objects, so its first crossing is the
  same point.
- **What the pick sees.** Visible flags come from the `Scene` (both docks keep it), and
  each transform from its uniform as on screen (`setLocalFromWorld`), so a `matrix` edit
  in the Parameters dock moves the pick. **Other parameter edits do not:** the C++ field is
  built from the graph's file values. A uniform value cannot be written back into a graph
  param generically (DualC packs primitives into a flat `params` array, per op), so the
  pick of an object whose radius was edited in the dock uses the file's radius.
- **The highlight.** `sceneColor` records the id of the object that gave the colour and
  reads one scene uniform, `u_selected` (`kSelectedUniform`, a float id, 0 for none). It is
  not a binding: the Parameters dock does not show it, a re-lowering does not reset it, and
  `TraceRenderer` sets it at each render from `Viewport::setSelectedObject`. A single
  graph's shader has no such uniform; setting it does nothing. Tint and dim factors are
  `kSelectionTint` and 0.45, a first choice to judge on the reference GPU.

## Verified (2026-10-12, P01 T1)

On the development box (Quadro FX 3700, nouveau NV92, Mesa 26.0.8), and on llvmpipe
(`LIBGL_ALWAYS_SOFTWARE=1`):

- `scene_render_test`, 400×300: a one-object scene of `gyroid_box.json` against the graph
  alone differs in 16 of 120 000 px, by 1 level, on NV92, and in 0 px on llvmpipe. Three
  spheres coloured (0.9, 0.1, 0.1), (0.1, 0.9, 0.1) and (0.1, 0.1, 0.9) read back with mean
  colours (204, 75, 75), (76, 207, 76) and (77, 77, 208) on both drivers. Hiding, showing
  and recolouring left the counts at 3 links and 6 compiles.
- `--snapshot` of three graphs (a sphere at x = −2, a box, a sphere at x = 2) shows grey,
  orange and blue.
