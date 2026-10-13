# P07 — Render scaling: culling and asynchronous compile

Added by P02's go/no-go (`docs/roadmap/03-go-no-go.md` § Changes P03–P06 need, item 1a,
approved by the owner on 2026-10-13). It runs right after P02, before P03: neither task
depends on E-08. It closes E-23 and E-21 before P05 multiplies objects and structural
edits. Both are editor-side; DualC's codegen and trace framework stay untouched (rule 5).

### T1. Per-object bounding-box culling (E-23)

`lowerScene` writes `sceneSDF` and `sceneColor` (`docs/design/03-scene-lowering.md`).
Each visible object's term is skipped when the distance from `p` to its world box is
already larger than the current `d`: that distance is a lower bound of the object's, so
the union is unchanged. The box is a uniform pair per object (`u_o<id>_boxMin`,
`u_o<id>_boxMax`), as the visibility flag is, so a move (P03 T3) updates it without a
compile. Record the scheme in `docs/design/03-scene-lowering.md`, and close E-23 there.

Add a family of objects stacked in depth to `scripts/measure_viewport.py --objects`, so
the owner's re-measurement (T3) covers rays that meet many boxes.

#### Acceptance criteria
- `--snapshot` of the P01.4 scenes (1, 3, 5, 10 primitives; 1, 3 TPMS-in-box) differs from
  the image before the change by no more than the driver noise measured in P01 T1
  (16 px, 1 level, on NV92; 0 px on llvmpipe). A ctest checks one of them.
- `scene_render_test` still counts no compile and no link across hide, show and recolour,
  and a culling-box upload neither compiles nor links.
- The pick (`src/pick.cpp`) and the culling agree on what is visible: a ctest picks
  through a scene where the culling skips the far object.

#### Notes (P07.1, 2026-10-13)
Built: `lowerScene` culling, box bindings, the depth-stacked family, `scene_render_test`
steps 4–5. All three acceptance criteria are met: on the GTX, the snapshots are 0 px off
(owner, dd92129); the culling-box upload compiles and links nothing; the pick agrees with
the image. The owner's GTX run shows the primitive slope up, +0.28 → +0.40 ms, and the
cold compile tripled (`docs/roadmap/02-measurements.md` § With per-object culling).
On the owner's answer (option C), only costly objects are culled (TPMS, lattices, meshes,
…; `docs/design/03-scene-lowering.md` § The cost rule); a scene of primitives is P01's
shader again. The `main_window` failure on the GTX predates P07 and is tracked on its own.

### T2. Asynchronous shader compile (E-21)

A structural edit (open, add, delete, reload) no longer blocks the GUI thread for the GL
compile. The GLSL is generated off the GUI thread (DualC's codegen has no GL dependency).
The program is compiled and linked in a context shared with the viewport's, and the
viewport keeps drawing the old program until the new one links. A failed link keeps the old
image and shows the error, as a broken file does today. An editor-side program cache is
not built here: the driver's cache already brings a source seen before to 14–31 ms on the
GTX (`docs/roadmap/02-measurements.md`). Record the threading in
`docs/design/01-render-path.md`, and close E-21 there.

#### Acceptance criteria
- A ctest swaps a program while frames are drawn: no frame mixes the old scene's uniforms
  with the new scene's program, and the old image stays until the swap.
- Adding an object while another structural edit is still compiling gives the scene of the
  last edit, not of the first.
- The status bar still shows the compile ms; `--bench` still reports `compileMs`.
- The command-reference page says what the window shows while a scene compiles.

#### Notes (P07.2, 2026-10-13)
Built: `Viewport::buildScene` and its compile thread, `TraceRenderer::linkProgram` and
`adoptProgram`, the window's lowering moved into the build, and `async_compile_test`. All
four acceptance criteria are met on llvmpipe and NV92. `Viewport::setScene` stays
synchronous, so `--bench`'s `compileMs` means what it did. The window's responsiveness on
the GTX is T3's human check (`HANDOFF_P07_TASK2.md`).

### T3. Measure culling and compile on the GTX 1050 Ti — human check

The owner runs `scripts/measure_viewport.py --objects` on the laptop, as in P01 T4, at
the commit of T2, plus the depth-stacked family of T1.

#### Acceptance criteria
- The rows are in `docs/roadmap/02-measurements.md`, with the growth per added object
  beside P01.4's (+0.28 ms per primitive, +2.49 ms per TPMS-in-box, settled GPU at 1.0).
- The per-primitive slope of the planar family stays near P01.4's (+0.28 ms): since the
  owner's option C (P07.1), primitives are not culled. The TPMS-in-box slope falls below
  P01.4's +2.49 ms. *(Corrected by P07.3: the criterion as first written, "falls well
  below P01.4's", contradicts option C and the 6f3a8bd run, +0.29 ms.)*
- The cold compile of each scene (its first run, after the driver's shader cache is
  cleared) is beside P01.4's (670 ms for 10 primitives, 1210 ms for 3 TPMS-in-box).
- The owner adds a third TPMS-in-box in the live window and reports whether the window
  stays responsive while it compiles, and the compile ms the status bar shows.

#### Notes (P07.3, 2026-10-13)
Built: `--unit` on `scripts/measure_viewport.py`, so the rows read `P07.3`, and the owner's
checklist (`docs/roadmap/02-measurements.md` § How to measure (P07 T3 checklist)), which
clears the shader cache before the run and before the live-window check. Criterion 2 is
corrected above, and a cold-compile criterion added (P07.1 asked for the cold figure). The
measurement itself is the owner's (`HANDOFF_P07_TASK3.md`).

## Exit criteria
- Tasks T1 to T3 are met. `python scripts/check.py` is green.
- E-21 and E-23 are closed in `docs/decisions/03-deferred.md`.
- `docs/roadmap/README.md` names P03.
