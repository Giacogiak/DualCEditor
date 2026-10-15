# P01 — Spike B: several objects, show/hide, picking

**DONE (2026-10-12).** Waited on E-07 (picking mechanism, decided 2026-10-12: CPU pick).
The questions: how does the scene's cost grow with the number of objects on the GTX
1050 Ti, and can hiding and selecting avoid a recompile? Answers: about +0.29 ms per added
primitive and +2.5 ms per added TPMS-in-box, settled GPU at 1.0, which triggered E-23
(T4); hide, show, recolour and select are uniforms and never link (T1–T3). Design:
`../../design/03-scene-lowering.md`. Usage: `../../command_reference/01-viewport.md`
§ Several graphs, § Objects. Development-box conditions: `README.md`.

## T1. A scene of several objects in one shader

*Planned:* an editor-side `Scene` of objects (name, field graph, transform, visible flag,
colour), lowered to a single graph, a union of each object wrapped in `transform`,
compiled once; visibility and colour as uniforms; the object id in each object's uniform
names. Acceptance: a scene of 3 objects renders, each in its own colour; toggling
visibility neither recompiles nor relinks, checked by counting;
`docs/design/03-scene-lowering.md` written and indexed.

**DONE (2026-10-12, commit 4472f87).** Delivered: `src/scene.{h,cpp}`: `Scene`,
`SceneObject` (id, name, graph, meshDir, rigid `transform`, `visible`, `colour`),
`sceneColourFor`, `objectUniformName`, `lowerScene` → `LoweredScene` (one `GlslScene` plus
per-object binding indices and boxes), `assembleSceneShader`. Each object's graph is wrapped
in DualC's `transform`, compiled by `compileToGlsl` on its own in its local padded box, then
every identifier gets the object id (`f7` → `o2_f7`, `u_n3_radius` → `u_o2_n3_radius`,
`sceneSDF` → `o2_sdf`); the editor appends `u_o<id>_visible` / `u_o<id>_colour`, a
`sceneSDF` that is the min over visible objects (uniform branch) and a `sceneColor`
returning the nearest visible object's colour; the trace box is the union of every
object's moved box, hidden ones included. The colour patch: DualC's framework has one fixed
line, `vec3  base = vec3(0.80, 0.82, 0.86);`, which `assembleSceneShader` replaces with
`sceneColor(hp)` for a lowered scene only, and throws if the line is not there exactly once
(a pin bump that changes it fails `scene_render_test` loudly); a single graph's shader
stays DualC's byte for byte. `TraceRenderer::compileCount()`. `parseGraphText`,
`traceBounds`, `collectNodeOps` split out of `compileGraphText`; `GraphDirMeshResolver`
and `graphMeshDir` public; `parseGraphFile`. `--snapshot` and `--bench` take several
`graph` arguments, one object each. Tests: `scene` (GL-free: namespacing, unique names,
object uniforms, `localFromWorld`, the union box including hidden objects, `stepScale` and
`featureScale`, the patch, an unpatched single graph, errors naming the object, ids not
reused) and `scene_render` (GL off-screen, skips 77). 10 ctests.
Verified on the development box, `scene_render_test` at 400×300: a one-object scene of
`gyroid_box.json` against the graph alone differs by 16 / 120 000 px by 1 level on NV92
and 0 px on llvmpipe; three spheres coloured red, green, blue read back as means
(204, 75, 75), (76, 207, 76), (77, 77, 208) on both drivers; after 3 hides, 3 shows and 1
recolour, links 3 → 3 and compiles 6 → 6, and showing again restores the image pixel for
pixel.
- **Deviation from the plan, settled here:** not one DualC graph. A single `compileToGlsl`
  gives no object-of-hit for the colour, and DualC's DAG dedup would merge two equal
  objects into one function and one uniform set. Each object is compiled by DualC
  separately and renamed; no DualC change (`../../design/03-scene-lowering.md` § Why).
- *Wrong on the way:* the one-object scene was first asserted bit-identical to the graph
  alone; on NV92 the program differs so the driver's codegen does (16 px), and the test
  allows 0.1% of pixels and prints the worst difference. The plan's page number
  `02-scene-lowering.md` was taken by `../../design/02-graph-files.md`; design pages number by birth
  order, so it is `03`.
- *Open then, superseded:* rigid transforms only (P03 T3 added a uniform scale through
  DualC's `scale` node); a transform edited as a uniform left the trace box stale (P03 T3
  makes the trace box follow). *Still true:* with every object hidden each pixel marches
  the whole box; `sceneColor` re-evaluates every visible object once per hit pixel, so T4's
  growth figure includes it.
- *Owner check:* the three-file snapshot (grey sphere, orange box, blue sphere) was asked
  for on the laptop; the owner's T3 laptop session (below) used the same scene in the
  window.

## T2. Objects panel

*Planned:* a dock listing the objects with a visibility checkbox, a colour swatch and a
name; selecting a row selects the object; Delete removes it; a second dropped graph adds
an object instead of replacing the scene. Acceptance: two dropped files give two objects in
the panel and the viewport; show, hide, select and delete work from the panel; the command
reference is updated.

**DONE (2026-10-12, commit d76e561).** Delivered: `ObjectsPanel` (`src/objects_panel`), a
`QListWidget` with one row per object (checkbox, 16 px swatch with the shader's 1/2.2
gamma, name, the id in the tooltip), single selection, `Delete` by `QKeySequence::Delete`;
it only emits `visibilityToggled`, `selectionMoved`, `deleteRequested`. `MainWindow` holds a
`Scene`; every structural change (open, add, delete, a reloaded file) lowers it and calls
`Viewport::setScene`, and the new document is committed on `sceneApplied`, so a failed
lowering or link leaves the scene on screen. `openFile` replaces the scene with one object
(File ▸ Open, first argument); `addFile` adds one (drop, `File ▸ Add…`, further
arguments); a drop adds every graph file it carries. Edits survive re-lowering: bindings
of objects whose graph did not change keep their values (matched by binding range and
`paramKey`); a reloaded file's objects take the file's values. The watch covers every
object's file plus "waiting" files (a failed open or add, added by the save that fixes
it); files that change together reload as one attempt. `ParameterPanel` sections (an
object's name above its first binding) and `showValue`; graph pointers prefixed with
`/objects/<id>` for display since two graphs share `/root/...`. `Viewport::clearScene`
(deleting the last object shows the background, links nothing). The window takes several
`graph` arguments. Test `objects_panel` (GL, skips 77); `main_window` adapted. 11 ctests.
Verified on the development box (NV92, 800×500, preview off, the three side-by-side
graphs of the command reference): two drops (`a.fld`, then `b.fld` + `c.fld` in one drag)
give 3 rows and 3 objects, 7009 orange and 7292 blue px of 200 716; hiding `b` from its
checkbox then showing it: links 3 → 3, compiles 6 → 6, no orange while hidden, the shown
image equal pixel for pixel; Delete on the selected row removes it, its colour gone, the
selection cleared; the last Delete leaves `hasScene() == false`. 11/11 ctests, none
skipped.
- *Wrong on the way:* `main_window_test` encoded "a drop replaces"; under the new rule its
  second drop added the cube inside the gyroid, which hides it, so editing the cube
  changed no pixel. After the second drop the test now checks two objects and removes the
  gyroid; nothing else changed.
- *Open:* with no argument the window opens on the default sample, so the first drop adds
  to it and the user deletes it from the dock (replacing an untouched default would be
  friendlier; not a rule the plan states). No colour picker: the swatch is display only
  and the colour is edited in the Parameters dock. After `File ▸ Open` ids start again at
  1, so a selected object 1 stays "selected". A shader link failure of an added file is
  reported but the file does not wait for a fix (a parse or lowering failure does); DualC's
  codegen should not emit unlinkable code.
- *Owner check:* dock height above Parameters, the swatches, `Delete` ("Suppr") with the
  dock focused and a fourth dropped `.fld`: not separately recorded; the owner drove the
  dock on the laptop in T3's check.

## T3. Click to select

*Planned:* picking as E-07 decided; the selected object highlighted in the viewport and the
panel, both directions. Acceptance: clicking an object selects it and clicking empty space
clears the selection, the nearest object along the ray winning where objects overlap; a
ctest covers picking on a known two-object scene without GL.

**DONE (2026-10-12, commit 2c9de42).** Delivered: `src/pick.{h,cpp}`, the CPU pick:
`viewRay` is the trace shader's `rd` for a widget point; `ScenePicker(scene, lowered)`
builds each object's own graph with `FieldGraph::build` (local frame, own resolver);
`pick(ray)` takes the ray into each visible object's frame through `localFromWorld`, clips
it to the object's local trace box and marches it as the shader does (gradient-normalised
steps for a non-metric object, `TraceRenderer`'s step bounds from the object's box and
`featureScale`, then 20 bisections); the smallest `t` wins; `setVisible` and
`setLocalFromWorld` follow uniform edits. `LoweredObject` carries `stepScale` and
`featureScale` per object. Highlight: `sceneColor` records the id of the object that
coloured the hit and reads one scene uniform `u_selected` (`kSelectedUniform`,
`kSelectionTint`); the selected object is mixed 0.45 toward amber (1.0, 0.78, 0.20) and
the others dimmed to 0.45 while something is selected. It is not a binding:
`TraceRenderer::setSelectedObject` stores it and `render` uploads it every frame, so the
dock does not show it, the value carry-over does not touch it, and it survives a relink; a
single graph's shader has no `u_selected`. `Viewport::clicked` on a plain left press and
release within `QApplication::startDragDistance()`; `setSelectedObject` repaints once at
full resolution. **Behaviour change:** an orbit or pan starts only past the drag distance
and the whole movement then applies, so a click does not turn the view.
`MainWindow::pickAt` builds the picker lazily at the first click after a commit, syncs
visibility and transforms, picks, calls `selectObject`, which also sets the viewport's
selection, so viewport, dock and `selectionChanged` agree both ways. Test `pick`
(GL-free); `scene` checks the `sceneColor` id and `u_selected`; `objects_panel` gains a
click section. 12 ctests.
Verified: `pick_test`, two spheres of radius 0.5 at the origin and (0.3, 0, −2), camera at
z = 5: the centre ray picks the front one at t = 4.500000; with it hidden the back one at
7 − √0.16 within 1e-4; a gyroid-in-box (non-metric) is hit at t = 4.208384 with field
−3.3e-9, nothing before the hit inside (checked every 1e-3). `objects_panel_test` on NV92: a
click at the centroid of the orange pixels selects `b` in window and dock, changes the
image, leaves yaw and pitch unchanged; a click at (4, 4) clears the selection and the image
equals the unselected one; links 3 → 3, compiles 6 → 6 across every selection; a dock
selection changes the viewport image. 12/12 ctests.
- *Wrong on the way:* the test used `GraphDirMeshResolver` without including
  `graph_file.h`; failed to build, include added.
- *Open:* parameter edits other than `matrix` and `visible` (and, since P03 T3, the
  scale) are not seen by the pick: the C++ field is built from the file's values, and
  writing a uniform back into the graph needs per-op knowledge of DualC's flat `params`
  packing (`../06-dualc-questions.md`; P06's `exportGraph` later did write edits into the
  params editor-side, which the pick could reuse). The picker builds on the first click
  after each commit, a BVH build for a mesh object, unmeasured. The amber tint and 0.45 dim
  are first choices (weak on a yellow object, the others' dimming carries the signal; no
  outline since `sceneColor` has no view direction), to judge on the GTX. Shift+click was
  left free (P05 T1 took Ctrl+click and Shift+click in the dock; there is no multi-select in
  the viewport). The pick uses its own per-object step bounds, so on thin TPMS walls it can
  select an object whose wall the preview stepped over: the exact field's answer.
- **Owner check (2026-10-12, laptop), done:** the front object is selected where two
  overlap; a hidden object is skipped by a click.

## T4. Frame time against object count — owner check

*Planned:* the owner measures on the laptop at 0.35 and 1.0 in a 1280×800 window: 1, 3, 5
and 10 analytic primitives; 1 and 3 TPMS-in-box. Acceptance: the rows are in
`../02-measurements.md`; the handoff states the growth per added object and whether E-23
should be triggered.

**DONE (2026-10-12, two sessions; commit b011b24; no C++ changed).** Delivered:
`scripts/measure_viewport.py --objects`: writes one `.fld` per object and runs `--bench`
over six scenes at 0.35 and 1.0 (1, 3, 5, 10 primitives in a fixed 5×2 grid of slots 1.2
apart, alternately a sphere of radius 0.4 and a box of side 0.8, none overlapping, the view
framing the union; 1 and 3 TPMS-in-box, `gyroid_box.json` without its hole, 5 apart along
x); prints the table rows (unit `P01.4`) and four *per added object* lines (least-squares
slope of each median against the count); `--keep` prints the scratch folder. The raw
lines carry `scene` and `objects`.
**Owner's run (2026-10-12, GTX 1050 Ti, Windows 10 10.0.19045, driver 441.93, Qt 6.8.3,
Release at b011b24, mains, editor closed, nothing selected):** rows and growth in
`../02-measurements.md` (unit `P01.4`, § Against object count); raw
`../02-measurements-p01.jsonl`. Settled GPU at 1.0: primitives 0.59 / 1.06 / 0.87 / 3.16
ms for 1 / 3 / 5 / 10, TPMS-in-box 2.64 / 7.62 for 1 / 3; growth **+0.28 ms per
primitive, +2.49 ms per TPMS-in-box** (the owner's reading: +0.29 and +2.5; the 5-row below
the 3-row is noise on frames under 1.1 ms). **E-23 triggered** (the owner's threshold,
commit 8f23e06: any clear slope). Headroom: the worst scene, 3 TPMS-in-box, 7.6–8.7 GPU /
8.6–9.4 wall ms at 1.0; the moving wall time at 0.35 is 2.8–4.1 ms for every scene (the
fixed per-frame cost, not the trace). Cold compile, first run at 0.35: 157 / 265 / 413 /
670 ms for 1 / 3 / 5 / 10 primitives (about +57 ms each), 339 / 1210 ms for 1 / 3
TPMS-in-box; from the driver's cache 14–30 ms: input to P02 on E-21.
Verified on the development box (llvmpipe, `--size 160x100 --frames 4`, commit 2c9de42
plus the script): all twelve runs exit 0; settled GPU ms at 1.0 primitives 1.7 / 4.2 / 4.1
/ 15.9, TPMS-in-box 21.3 / 61.8; per added object +1.57 and +20.2 ms, which says nothing
about the GTX but shows the growth: every march step evaluates every visible object.
`--snapshot` of the ten primitives and the three boxes draws every object in its colour.
- *Wrong on the way:* the growth lines at 0.35 came out empty: `previewScale` in the bench
  JSON is 0.3499999940395355 and was compared with `==`; now within 1e-3.
- *Open then:* the primitive scenes all lie in z = 0, so most rays meet one or two objects
  (P07 T1 added a depth-stacked family); no mesh objects, so E-22's trigger was unmeasured
  (P04 T2 added `--meshes`); the cold compile grows about 57 ms per primitive, several
  seconds for the scenes P05 builds (P02 weighed it, P07 T2 built the asynchronous
  compile).

## Exit criteria

T1 to T4 met as above; `python scripts/check.py` green (12/12 ctests); `../README.md`
named P02.

---
← Back to the [Plan record](README.md) · [Roadmap index](../README.md).
