# P07 — Render scaling: culling and asynchronous compile

**DONE (2026-10-13).** Added by P02's go/no-go (`../03-go-no-go.md` § Changes P03–P06
need, item 1a, approved 2026-10-13) and run before P03, since neither task depends on
E-08. It closed E-23 and E-21 (`../../decisions/03-deferred.md`) before P05 multiplied
objects and structural edits. Both are editor-side; DualC's codegen and trace framework
were not touched (rule 5). Design: `../../design/03-scene-lowering.md` § Culling and § The
cost rule, `../../design/01-render-path.md` § Asynchronous compile. Numbers:
`../02-measurements.md` § With per-object culling and the two GTX sections after it. One
result stayed open: the cold compiles at the end of P07 are longer than P01.4's, cause
unknown (T3).

## T1. Per-object bounding-box culling (E-23)

*Planned:* each visible object's term in `sceneSDF` and `sceneColor` is skipped when the
distance from `p` to its world box is already larger than the current `d` (a lower bound
of the object's distance, so the union is unchanged); the box is a uniform pair per object
(`u_o<id>_boxMin`, `u_o<id>_boxMax`) so a move (P03 T3) updates it without a compile;
add a family of objects stacked in depth to `--objects`. Acceptance: `--snapshot` of the
P01.4 scenes differs from before by no more than P01 T1's driver noise (16 px, 1 level on
NV92; 0 on llvmpipe), a ctest checking one; `scene_render_test` still counts no compile and
no link across hide, show, recolour and a box upload; the pick and the culling agree on
what is visible, a ctest picking through a scene where the culling skips the far object.

**DONE (2026-10-13; commits dd92129 culling all objects, 6f3a8bd option C).** Three
sessions. The first returned blocked: the development box's GL stack had wedged (below);
the owner rebooted it. The second recorded the owner's GTX run at dd92129 and returned two
decisions; the third built the owner's option C.
Delivered: `lowerScene(scene, cull = true)`; two more bindings per object, `boxMin` /
`boxMax` (vec3, pointer `/objects/<id>`, `LoweredObject::boxMinBinding` / `boxMaxBinding`,
shown greyed out in the Parameters dock); each term becomes
`if (visible) { float b = sceneBoxDist(p, boxMin, boxMax); if (b <= max(d, 0.0)) ... }`;
`cull = false` writes P01's plain union byte for byte, the tests' reference;
`worldBox(local, worldFromLocal)` public; a `matrix` edit in the dock uploads the moved
box. `--objects` gains the family *N stacked in depth* (1, 3, 5, 10 spheres and boxes in a
column along the default view axis, 1.2 apart, the first nearest the camera).
`scene_render_test` steps 4–5: a box upload neither compiles nor links and the culling
reads the boxes; culled images equal the plain union's; in two objects stacked in depth
the CPU pick and the culled image agree pixel by pixel.
**Option C (the owner, 2026-10-13):** cull only costly objects. `isCostlyObject(graph)`,
`LoweredObject::culled`. Cheap, never culled: a graph of at most 8 nodes
(`kCheapObjectNodes`), all analytic primitives (`sphere`, `box`, `roundbox`, `ellipsoid`,
`torus`, `capsule`, `cappedcylinder`), sharp booleans or placement (`translate`, `rotate`,
`transform`, `scale`, `mirror`). Costly, culled: anything else (TPMS, lattices, `mesh`,
smooth booleans, `onion`, `offset`, `round`, `normalize`, warps, `repeat`, more than 8
nodes). A cheap object is written as in P01 with no box uniform declared, and a scene with
no costly object omits `sceneBoxDist` too, so its shader is P01's byte for byte
(`scene_test`). The box bindings stay on every object, so indices, P03's move and the dock
do not depend on the rule. `scene_render_test` step 4 uses three thin shells
(`onion(sphere)`, costly) and checks spheres are not culled; step 5 stacks two shells.
Verified on the development box, `scene_render_test` 400×300: culled against plain union
0 px for three spheres, three shells and gyroid_box on llvmpipe and NV92; pick against
culled image 7500 / 7500 on llvmpipe, 7486 agree and 14 disagree (0.19%, silhouette
pixels, bound 0.5%) on NV92; 4 links and 8 compiles before and after the box uploads;
12/12 ctests. After the reboot the first full gate failed `viewport` (timing-based) once on
a box just rebooted; it passed alone and on the second run.
**Owner's GTX runs (2026-10-13, Windows 10, driver 441.93, Qt 6.8.3, Release),** tables in
`../02-measurements.md`, raw `../02-measurements-p07.jsonl` (first 20 lines dd92129, next
20 6f3a8bd):
- at dd92129 (culling all): `--snapshot` at 8b9baf8 against dd92129, every `--objects`
  scene, **0 / 480 000 px**; settled GPU per added object at 1.0, P01.4 → dd92129:
  primitives +0.28 → +0.40 ms, TPMS-in-box +2.49 → +1.90, stacked in depth +0.18 (new);
  cold compile at 0.35: 10 primitives 670 → 2009 ms, 3 TPMS-in-box 1210 → 3503; the
  window with 3 TPMS-in-box: nothing missing, orbiting fine; ctest 11/12, `main_window`
  fails "the .fld renders a different image" (`tests/main_window_test.cpp:220`), the same
  at 8b9baf8, so it predates P07. Reading: the culling is exact but does not pay for cheap
  primitives (the box test costs about a sphere's SDF; the first visible object in scene
  order is always evaluated), and the non-uniform branches tripled the cold compile.
- at 6f3a8bd (option C): **0 / 480 000 px** in each of the ten scenes, byte-identical to
  the 8b9baf8 images; primitive slope **+0.29 ms** (expected about +0.28), TPMS-in-box
  **+1.94** (expected near +1.90), stacked in depth +0.12 (a cheap-object baseline now);
  wall times back to the 2026-10-12 level. **Not confirmed: the cold primitive compile**:
  every compile in this run took 12–60 ms, so the driver's disk cache served them; the
  cold figure became T3's.
**The owner's decisions on this unit:** P07.1 accepted with the GTX `main_window` failure
open as its own item (to be investigated with the GTX in the loop; the GTX ctest stays one
short until then; it does not block P07.2); culling option C. A two-pass scheme (option B)
was offered and not chosen; it is the lever if a later phase needs more.
- *Wrong on the way:* the first version clipped each evaluated field to its box,
  `max(o_sdf, b)`; on llvmpipe it changed gyroid_box's image (2875 px, up to 76 levels)
  because that graph's field is below the box distance near the box's edges: removed, the
  culling only skips. The first box-upload test moved object 1's box, which is never culled
  (nothing is nearer yet); it moves the last object's. NV92 snapshots and a GL ctest were
  run in parallel: the culled 10-object shader ran for minutes on NV92, a
  `scene_render_test` then waited on the GPU at 0% CPU and was killed, and after that no GL
  program started on the box, software or not, until the reboot. `pkill -f` with a pattern
  matching the shell killed the shell twice.
- *Facts found:* **NV92 (nouveau) draws scenes of 5 or more objects wrong** at 8b9baf8,
  before the change (480 000 / 480 000 px differ from llvmpipe for 5 and 10 primitives and
  3 TPMS-in-box). The development box cannot judge many-object images; llvmpipe can.
  llvmpipe snapshots of 1 and 3 objects took about a second each.
- *Open:* the GTX `main_window` failure (`../README.md`); whether the culled shader or the
  parallel GL runs hung NV92 (since the reboot NV92 passes the gate with the culled
  shells); with costly objects the first one in scene order is still always evaluated.

## T2. Asynchronous shader compile (E-21)

*Planned:* a structural edit (open, add, delete, reload) no longer blocks the GUI thread
for the GL compile; the GLSL is generated off the GUI thread (DualC's codegen has no GL
dependency); the program is compiled and linked in a context shared with the viewport's,
which keeps drawing the old program until the new one links; a failed link keeps the old
image and shows the error; no editor-side program cache (the driver's cache already brings
a seen source to 14–31 ms). Acceptance: a ctest swaps a program while frames are drawn, no
frame mixing the old uniforms with the new program and the old image staying until the
swap; adding an object while another edit is compiling gives the last edit's scene; the
status bar still shows the compile ms and `--bench` still reports `compileMs`; the command
reference says what the window shows during a compile.

**DONE (2026-10-13, commit 4b89008).** Delivered: `TraceRenderer::setScene` split into a
static `linkProgram(gl, scene, …)` (compiles and links on any current context, timed as
before) and `adoptProgram(program, scene, ms)` (swaps program, grids, bindings and trace
constants in one step), plus `countElsewhere` and `dropProgram`; `setScene` is both, so its
counters are unchanged. `Viewport::buildScene(make, frame, finish)`: a compile thread
(`QThread::create`) owns a `QOpenGLContext` shared with the widget's on a
`QOffscreenSurface`, runs `make` (DualC's codegen), `linkProgram`, `glFinish`, and queues
the result to the GUI thread, where `finish` edits values, `adoptProgram` runs and
`sceneApplied` follows; tickets: every `buildScene`, `setScene` and `clearScene`
supersedes the build in flight, the thread takes only the newest waiting build, stale
results are dropped and their programs deleted; `releaseGL` stops the thread (waiting out
a link under way) and restarts a build in flight on the next context; `building()` and
`buildStarted`; `setScene` stays synchronous. `MainWindow::apply` passes `lowerScene` to
`buildScene`; the value carry-over moved to `carryValues`, run at the swap, copying the
values on screen (edits made during the compile included) and taking visible flags and
colours from the pending document; a `Pending` replacing another inherits its attempts,
its `fresh` set and its `waits`; hide, show, recolour and the pick work during a compile;
the status bar reads `compile …` while building. ctest `async_compile`.
Verified: criterion 1, every frame grabbed during a build (400 ms codegen sleep) equals
the old scene's image and the first frame after the swap equals the new scene's drawn
through `setScene`, 0 px; criterion 2, add a, then add b and add a again while the build is
in flight gives three objects and 3 × `loadFinished(true)`, a hide during the compile is
kept, and on a bare Viewport two builds in flight give one `sceneApplied` with the last
one's image; criterion 3, `compile C ms` returns once the scene is on screen and the
`bench` ctest passes; criterion 4, the command reference § While a scene compiles. On the
development box, 320×240: NV92 5 frames and 14 ticks of a 5 ms timer during a 400 ms
build; llvmpipe 3 frames and 6 ticks (a gyroid frame grab takes about 80 ms on NV92). 13/13
ctests.
**Owner's GTX run at 4b89008 (2026-10-13), done:** with 2 TPMS-in-box open, a third
`tpms_box_2.fld` dropped in: the window kept orbiting, showed `compile …`, then the
compile ms ("all good"; the figure was not written down and most likely came from the
driver's cache; the cold figure is T3's). `async_compile` 17/17. ctest 12/13, `main_window`
failing the same check as at every earlier commit. Owner's acceptance: the window stays
responsive during an async compile on the GTX.
- *Wrong on the way:* the first frame threshold (≥ 5 frames during the build) only just
  passed on NV92 and failed on llvmpipe's slower grabs; now ≥ 2 frames and ≥ 5 timer
  ticks.
- *Open:* whether the driver links slower on a second context, or serialises the two
  contexts, was not verified (T3 saw 2567 ms on the compile thread against 1897 ms in the
  bench for the same scene, one sample each). The GTX `main_window` failure may be a timing
  race: the test's `quiet()` waits 500 ms with no paint, which a slow compile can outlast;
  the window path is now asynchronous, so a slow compile with no paint can still beat it;
  the test was not edited. Edge case: if an add fails at lowering after another add
  replaced it, both files go to the watch list and each is added only by its own next save.
  The reparent path (`initializeGL` with a scene on screen) still relinks synchronously.

## T3. Measure culling and compile on the GTX 1050 Ti — owner check

*Planned:* the owner re-runs `--objects` at T2's commit or later, plus T1's depth-stacked
family. Acceptance: the rows are in `../02-measurements.md` with the growth beside P01.4's
(+0.28 ms per primitive, +2.49 per TPMS-in-box); the per-primitive slope of the planar
family stays near P01.4's (primitives are not culled under option C) and the TPMS-in-box
slope falls below +2.49 (*corrected by P07.3: first written "falls well below P01.4's",
which contradicts option C and the 6f3a8bd run*); the cold compile of each scene, after
the driver's shader cache is cleared, is recorded beside P01.4's (*added by P07.3*); the
owner adds a third TPMS-in-box in the live window and reports whether it stays responsive
and the compile ms shown.

**DONE (2026-10-13; commits b163141 script and checklist, 3712bde the record).**
Delivered: `scripts/measure_viewport.py --unit ID` (rows had been labelled `P01.4` in T1's
runs); `../02-measurements.md` § How to measure (P07 T3 checklist), which clears the
driver's cache before the script and again before the live-window check; the plan's
criterion 2 corrected and criterion 4 added as above. The driver read the corrected
criterion 2 as the owner's option A (keep both edits); the owner did not pick an option on
that question and never explicitly confirmed it.
**Owner's run (2026-10-13, GTX 1050 Ti at b163141, same C++ as 4b89008, Windows 10, driver
441.93, Qt 6.8.3, mains, `%LOCALAPPDATA%\NVIDIA\GLCache` (138 files, 30.7 MB) emptied
before the script):** tables in `../02-measurements.md` § GTX 1050 Ti at b163141; raw, the
last 20 lines of `../02-measurements-p07.jsonl` (BOM stripped, CRLF kept). Per added
object, settled GPU at 1.0: primitives **+0.29** (0.35: +0.34; P01.4 +0.28), stacked in
depth **+0.13** (+0.11), TPMS-in-box **+1.96** (+1.95; P01.4 +2.49). Cold compile, first
run at 0.35, GUI thread, b011b24 → b163141: 1 primitive 157 → 152, 3 primitives 265 → 399,
5 primitives 413 → 512, 10 primitives 670 → **1137**, 1 TPMS-in-box 339 → 757, 3
TPMS-in-box 1210 → **1897** ms. Live window, cold (cache emptied again):
`DualCEditor tpms_box_0.fld tpms_box_1.fld`, then `tpms_box_2.fld` dropped during a held
orbit: the view kept turning while the status bar read `compile …` ("all good"), then
`shader compile 2566.6 ms`, timed on the compile thread.
Criteria: 1 met; 2 met as corrected (+0.29 against +0.28; +1.96 below +2.49); 3 met
(responsive, 2567 ms); 4 recorded, and the figures are longer, which is open, not a pass.
- *Wrong on the way:* the first draft left open whether the window's 2567 ms includes the
  codegen; it does not: `TraceRenderer::linkProgram` starts its clock after
  `assembleSceneShader` and stops at the link status on both paths, so the two figures
  time the same span. The owner's README listed P01.4's 1 TPMS-in-box cold compile as "—";
  `../02-measurements.md` gives 339 ms, which the table uses.
- **Open: the cold compiles are longer than P01.4's, cause unknown.** 10 primitives 1137
  against 670 ms although option C gives primitives P01's shader. Candidates, none
  checked: the generated source differs from b011b24's in a way not intended (box uniforms
  declared, or a changed `sceneColor`); the driver differed on that run; noise (one sample
  of each). **A first check that needs no GTX:** diff the fragment source of the
  10-primitive scene at b011b24 and at HEAD on the development box. TPMS-in-box 757 / 1897
  against 339 / 1210: the culling branch around each costly SDF accounts for part (T1 saw
  cold compiles triple when every object was culled). The window's compile thread, 2567 ms
  against the bench's 1897 for the same 3-TPMS scene: a slower link on a second shared
  context would fit (T2). The window stays responsive, so none of this blocked P03. The
  stacked-in-depth compiles (4–36 ms) were not cold; they likely share a source with the
  side-by-side scenes, unchecked. `../04-gtx-batch-2026-10-13.md` § 9 asks for a second
  cold sample.

## Exit criteria

T1 to T3 met as above; `python scripts/check.py` green (13/13 ctests); E-21 and E-23
**Closed (P07.3, 2026-10-13)** in `../../decisions/03-deferred.md`; `../README.md` named P03.

---
← Back to the [Plan record](README.md) · [Roadmap index](../README.md).
