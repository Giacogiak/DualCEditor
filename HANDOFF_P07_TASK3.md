# Handoff — P07 T3: measure culling and compile on the GTX 1050 Ti (human check)

Date: 2026-10-13. Plan: `plan/35_P07_render_scaling.md` § T3. DualC pin: `2fcb6ee` (unchanged).

**Status: blocked on the owner's GTX run.** The tooling and the checklist are ready. The
rows, the slopes, the cold compile and the live-window check have to come from the laptop
(E-04). No C++ changed, so a build at 4b89008 or later measures T2's code.

## What changed and why

- **`scripts/measure_viewport.py --unit ID`** sets the unit id printed in each row. The
  default is unchanged (`P00.5`, or `P01.4` with `--objects`). P07 T1's GTX runs were
  labelled `P01.4` by the script, which made them easy to confuse with P01's rows.
- **`docs/roadmap/02-measurements.md` § How to measure (P07 T3 checklist).** It clears the
  driver's shader cache before the script, so each scene's first run (at 0.35) is a cold
  compile. The 6f3a8bd run skipped this, and its 12–60 ms compiles came from the cache. It
  clears the cache again before the live-window check, so the status bar's compile ms for
  the third TPMS-in-box is cold too.
  - Windows: `%LOCALAPPDATA%\NVIDIA\GLCache`.
  - Linux: `~/.nv/GLCache`.
- **`plan/35_P07_render_scaling.md` § T3, criterion 2 corrected.** It read "the
  per-primitive slope of the planar family falls well below P01.4's". Since the owner's
  option C (P07.1), primitives are not culled, so their shader is P01's. The 6f3a8bd run
  measured +0.29 ms against P01.4's +0.28. It now reads: the planar slope stays near
  P01.4's, and the TPMS-in-box slope falls below +2.49 ms.
- **Criterion 4 added:** the cold compile of each scene is recorded beside P01.4's.
  P07.1's notes left the cold figure to T3. This is asked of the owner as question 2.
- **Docs:**
  - `docs/command_reference/01-viewport.md` names `--unit` and the depth-stacked family.
  - `docs/roadmap/README.md`: T2 is done, T3 waits on the GTX run.
  - P07.3's notes are in the plan.

## Measured (development box, tool check only, not a performance figure — E-04)

`LIBGL_ALWAYS_SOFTWARE=1 python scripts/measure_viewport.py --objects --unit P07.3 --size 160x100 --frames 4`.
Conditions: llvmpipe (Mesa 26.0.8, LLVM 21.1.8), Qt 6.10.2, commit 5d34d7e plus the script
change.
- It ran 20 scenes end to end in 52 s, with every row labelled `P07.3`.
- Settled GPU per added object at 1.0: primitives +1.36 ms, stacked in depth +0.29 ms,
  TPMS-in-box +18.98 ms. P01.4's llvmpipe tool check gave +1.57 and +20.2.
- 3 TPMS-in-box compiled in 517 ms cold at 0.35, and 80 ms at 1.0 from Mesa's cache.

## Exit criteria of P07, one by one

| Bullet | State | Evidence |
|---|---|---|
| T1 met | yes | 0 / 480 000 px snapshot diff on the GTX at dd92129 and 6f3a8bd. `scene_render_test` steps 4–5. Pick test. Owner passed P07.1 (2026-10-13). |
| T2 met | yes | `async_compile` 17/17 on the GTX at 4b89008. The owner saw the window keep orbiting during a compile. Owner passed P07.2. |
| T3 met | **no — owner's run** | Checklist ready. Needs the twenty rows, the six slopes, the cold compiles and the live-window answer. |
| `check.py` green | yes | `python scripts/check.py` exit 0 on NV92 at this commit, 13/13 ctests. |
| E-21, E-23 closed in `03-deferred.md` | **not yet** | Both read *Built*. E-23 says "closes when P07 T3 measures the slope". E-21's responsiveness check passed at 4b89008, but its cold compile figure is T3's. I close both once the numbers are in. |
| `docs/roadmap/README.md` names P03 | **not yet** | It names P03 under *Next up*. The focus moves to P03 when P07 closes. |

## What I got wrong on the way

Nothing went wrong in this unit. One thing to watch: the window's 3-TPMS shader can be the
same source as the script's 3-TPMS scene. If so, the driver's cache would serve it after
step 2, which is why the checklist clears the cache a second time.

## Open

- **The owner's GTX run** (checklist steps 1–4).
- **The `main_window` failure on the GTX** (`tests/main_window_test.cpp:220`) is still its
  own item, not part of T3.
- **E-21's program cache:** whether it is still unbuilt depends on the cold compile ms. If
  the owner finds a cold 3-TPMS compile too long even off the GUI thread, that is a new
  trigger for the editor cache. The plan did not build one.

## For the next unit

When the numbers arrive:
1. Append the raw JSON to `docs/roadmap/02-measurements-p07.jsonl` and list it in
   `docs/roadmap/README.md`.
2. Add a § GTX 1050 Ti at T2's commit (P07 T3) table to `02-measurements.md`, beside
   b011b24, dd92129 and 6f3a8bd. Include the cold compiles.
3. Close E-21 and E-23 in `docs/decisions/03-deferred.md`.
4. Move the roadmap focus to P03, which waits on E-08 (the driver stops there).
