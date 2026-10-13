# Handoff — P07 T3: measure culling and compile on the GTX 1050 Ti (human check)

Date: 2026-10-13. Plan: `plan/35_P07_render_scaling.md` § T3. DualC pin: `2fcb6ee` (unchanged).

**Status: done.** The owner measured on the GTX at b163141 with a cold shader cache.
E-21 and E-23 are closed, P07 is closed, and the roadmap focus is P03. One result is
**open**, not a pass: the cold compiles are longer than P01.4's, and the reason is not
known (§ Open).

## What changed and why

First session (b163141):
- **`scripts/measure_viewport.py --unit ID`** sets the unit id printed in each row. The
  default is unchanged (`P00.5`, or `P01.4` with `--objects`). P07 T1's runs had been
  labelled `P01.4`.
- **`docs/roadmap/02-measurements.md` § How to measure (P07 T3 checklist).** It clears the
  driver's shader cache before the script and again before the live-window check. P07 T1's
  6f3a8bd run skipped this, so its compiles came from the cache.
- **Plan, T3 criterion 2 corrected.** "The planar slope falls well below P01.4's" became
  "stays near P01.4's, and the TPMS-in-box slope falls below +2.49 ms". Under option C,
  primitives are not culled.
- **Plan, T3 criterion 4 added:** the cold compiles are recorded beside P01.4's.
- **Docs:** the command reference names `--unit`.

After the owner's run (this commit):
- **`docs/roadmap/02-measurements.md` § GTX 1050 Ti at b163141 (P07 T3, cold cache):** the
  slope table beside b011b24, dd92129 and 6f3a8bd, the cold-compile table, the live-window
  answers and the findings.
- **`docs/roadmap/02-measurements-p07.jsonl`:** the owner's 20 raw lines are appended, as
  the last 20 lines. The BOM was stripped and CRLF endings kept, as in the file.
- **`docs/decisions/03-deferred.md`:** E-21 and E-23 are **Closed (P07.3)**.
- **`docs/roadmap/README.md`:** the focus is now P03, which waits on E-08. The cold-compile
  item is open. P07 is listed as done. The jsonl's file entry is updated.
- **Plan:** P07.3's notes.

## Measured (owner, GTX 1050 Ti, 2026-10-13)

Conditions: b163141, the same C++ as 4b89008, and the build matched. Windows 10, driver
441.93 (GL 3.3.0), Qt 6.8.3, Release, mains power, no other editor window. 1280×800,
60 frames per phase. `%LOCALAPPDATA%\NVIDIA\GLCache` (138 files, 30.7 MB) was emptied before
the script. The bundle is `.plan-run/P07_T3_GTX_2026-10-13_b163141.zip`, which is not
tracked.

Per added object, settled GPU ms:

| Family | at 1.0 | at 0.35 | P01.4 at 1.0 |
|---|---|---|---|
| primitives (1, 3, 5, 10) | +0.29 | +0.34 | +0.28 |
| stacked in depth (1, 3, 5, 10) | +0.13 | +0.11 | — |
| TPMS-in-box (1, 3) | +1.96 | +1.95 | +2.49 |

Cold compile, first run at 0.35, GUI thread (`--bench`), ms:

| Scene | b011b24 (P01.4) | b163141 |
|---|---|---|
| 1 primitive | 157 | 152 |
| 3 primitives | 265 | 399 |
| 5 primitives | 413 | 512 |
| 10 primitives | 670 | 1137 |
| 1 TPMS-in-box | 339 | 757 |
| 3 TPMS-in-box | 1210 | 1897 |

**Live window, cold** (the cache emptied again):
- Setup: `DualCEditor tpms_box_0.fld tpms_box_1.fld`, then `tpms_box_2.fld` dropped during
  a held orbit.
- The view kept turning while the status bar read `compile …`. The owner said "all good".
- Then the status bar read `shader compile 2566.6 ms`, timed on the compile thread.

## Acceptance criteria of T3

1. **The rows are in `02-measurements.md`**, with the growth per added object beside
   P01.4's. Met (the section above, in that file).
2. **The planar slope stays near P01.4's** (+0.29 against +0.28), **and the TPMS-in-box
   slope falls below +2.49** (+1.96). Met. This is the criterion as corrected in the first
   session. The owner did not pick an option on that question. The driver read the answer
   as option A, keep both edits. **The owner should confirm this.**
3. **A third TPMS-in-box added in the live window:** the window stayed responsive, and the
   compile ms was 2567. Met.
4. **The cold compiles are beside P01.4's.** Recorded. They are longer, which is open
   below, not a pass.

## Exit criteria of P07, one by one

| Bullet | State | Evidence |
|---|---|---|
| T1 met | yes | 0 / 480 000 px snapshot diff on the GTX at dd92129 and 6f3a8bd. `scene_render_test` steps 4–5. Pick test. The owner passed P07.1. |
| T2 met | yes | `async_compile` 17/17 on the GTX at 4b89008. The owner passed P07.2. |
| T3 met | yes | Above. Criterion 2's wording awaits the owner's confirmation. |
| `check.py` green | yes | `python scripts/check.py` exits 0 on NV92 at this commit, 13/13 ctests. |
| E-21, E-23 closed in `03-deferred.md` | yes | Both carry **Closed (P07.3, 2026-10-13)**. |
| `docs/roadmap/README.md` names P03 | yes | *Now: P03, scene model*, waiting on E-08. |

## What I got wrong on the way

- The first draft of this handoff left open whether the window's 2567 ms includes the
  codegen. It does not. `TraceRenderer::linkProgram` starts its clock after
  `assembleSceneShader` and stops it at the link status, on both paths (the compile thread
  and `--bench`). So the two figures time the same span, and the gap is not the codegen.
- The owner's README lists P01.4's 1 TPMS-in-box cold compile as "—". `02-measurements.md`
  § Against object count gives 339 ms, and that is what the table uses.

## Open

- **The cold compiles are longer than P01.4's, cause unknown.**
  - 10 primitives: 1137 against 670 ms, although option C gives primitives P01's shader
    (`docs/design/03-scene-lowering.md` § The cost rule). Candidates, none checked:
    - the generated source differs from b011b24's in a way not intended (box uniforms
      declared, or a changed `sceneColor`);
    - the driver differs on that run;
    - noise. There is one sample of each.

    A first check: diff the fragment source of the 10-primitive scene at b011b24 and at
    HEAD. On the dev box the source can be dumped, so this does not need the GTX.
  - TPMS-in-box: 757 / 1897 against 339 / 1210 ms. The culling branch around each costly
    SDF accounts for part of this (P07.1 saw cold compiles triple when every object was
    culled).
  - The window's compile thread takes 2567 ms against the bench's 1897 ms for the same
    3-TPMS scene. A slower link on a second, shared context would fit
    (`HANDOFF_P07_TASK2.md` § Open). There is one sample of each.
  - The window stays responsive, so none of this blocks P03.
- **The stacked-in-depth compiles (4–36 ms) were not cold.** They likely share a source
  with the side-by-side scenes, but that was not checked.
- **The `main_window` failure on the GTX** (`tests/main_window_test.cpp:220`) is still its
  own item.
- **Criterion 2's corrected wording** awaits the owner's explicit confirmation (option A).

## For the next unit

- The next phase is P03, which waits on E-08 (the scene file format,
  `docs/decisions/02-open.md`). The driver stops there.
- P03 T3 (gizmo): a move is a uniform upload, including the culling box
  (`u_o<id>_boxMin`/`boxMax`). It works during a compile. A structural edit goes through
  `MainWindow::apply` → `buildScene`. Wait for `sceneApplied` / `loadFinished` before
  reading the new scene.
