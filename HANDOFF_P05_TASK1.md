# Handoff — P05 T1: booleans and smooth booleans

Date: 2026-10-13. Plan: `plan/60_P05_operations.md` § T1. DualC pin: `2fcb6ee` (unchanged).

**Status: done, human check owed.** The new `Operations` menu has `Union`, `Intersection`,
`Difference`, `Xor`, `Smooth Union`, `Smooth Intersection`, `Smooth Difference`, and a
`Blend k` spin box (mm, default 0.25, which is DualC's default). Each acts on the objects
selected in the Objects dock, in the order they were selected. The dock now takes Ctrl+click
and Shift+click. Over more than two operands a boolean folds left, `op(op(a, b), c)`. Under
a smooth boolean, a non-metric operand is wrapped in `normalize` (E-10, decided: wrap
automatically). The undo step and the log name each wrapped operand. `python scripts/check.py`
exits 0 on the dev box (NV92), with 19/19 ctests.

## What changed and why

- **`operationGraph` (`src/scene_document.cpp`):**
  - **The fold.** DualC's booleans take exactly two children (`requireChildren(n, 2)` in
    `field_graph.cpp`). Before this change, `applyOperation` over three objects built a
    three-input `union` that DualC rejects. Every node of the fold carries the same params.
    The pointers stay structural: with three operands, operand 0 sits at `/root/in/0/in/0`,
    operand 1 at `/root/in/0/in/1` and operand 2 at `/root/in/1`. With two operands the
    pointers are the same as before, so P03's tests and saved scenes are unchanged.
  - **The E-10 wrap.** When `op` is a smooth boolean and `isMetricGraph(operand)` is false,
    the operand goes under `normalize`. The wrap is outermost, above `transform` and
    `scale`, and `normalized` returns the indices of the wrapped operands.
- **New helpers:** `isBooleanOp`, `isSmoothBooleanOp` (both through DualC's `parseBoolOp`)
  and `isMetricGraph`. `isMetricGraph` returns false when the graph holds any of these with
  no `normalize` above it:
  - a `winding`;
  - a TPMS token from `dce::tpmsKinds()`;
  - a `twist`, `bend` or `displace`. These have a step scale below 1 in DualC's
    `nodeStepScale`, as TPMS and winding do.

  A `mesh` counts as metric. It is a signed distance near the surface, which is the only
  region a blend of width `k` reads, and wrapping it would add `normalize`'s gradient
  samples to the mesh grid's cost for nothing. Strut lattices are distances, so they count
  as metric too.
- **`SceneDocument::applyOperation`:** the step's text is `"<name> (normalize on a, b)"`
  when a wrap happened. This text is the history that E-10's chosen option asks for.
- **`ObjectsPanel`:**
  - `ExtendedSelection`, with `order_` holding the selection order. `syncOrder` drops the
    rows that are no longer selected and appends new ones in row order.
  - `selectedIds()` returns that order. `selectedId()` is now the last selected id, which
    is the object the gizmo, the Transform dock and the highlight follow.
  - `selectInOrder(ids)` is used by tests.
  - `select(id)` is `ClearAndSelect`.
  - `setObjects` keeps the whole ordered selection across a rebuild.
  - The new signal `selectionOrderChanged` keeps the menu's enabled state current.
- **`MainWindow`:**
  - The `Operations` menu. Its actions carry the op token in `data()`.
  - `applyBoolean(op)`, plus `smoothK()` and `setSmoothK()`.
  - The actions are enabled when at least two selected objects are in both the document and
    the scene on screen.
  - `selectAfter_` selects the result when the scene that holds it commits, and is reset
    when the scene fails.
  - The log and status line read like
    `smooth-union of open.obj, closed.obj, k 0.5 mm; normalize on open.obj, not distance fields (E-10)`.
- **Tests:**
  - **New `operations` ctest (GL-free).** It uses five objects: a sphere, a box moved and
    scaled 1.5, an imported open box (`winding`), a clipped raw gyroid and a clipped octet
    lattice. It runs all 7 ops over 5 selections: (sphere, box), (box, sphere),
    (sphere, winding), (octet, gyroid) and (sphere, octet, box). For each result it checks:
    1. The result's graph is dumped and re-read as `dualc_field` reads a graph (`dumpJson`,
       then `parseJson`), then built by DualC. Its field equals, at 216 points, the field
       of the graph written by hand in DualC's shorthand: operands posed, folded left,
       non-metric ones under `normalize` for the smooth ops.
    2. The step text names the normalize exactly when one was added.
    3. The operands are hidden.
    4. Undo gives back a scene equal to the one before (`sameScene`), and redo gives back
       the same graph.
    5. The sharp `union`, `intersection` and `difference` over two operands equal `min`,
       `max` and `max(a, -b)` of the operands' own fields.

    Finally, smooth union and smooth difference over (sphere, winding) and (octet, gyroid)
    lower to GLSL.
  - **`main_window`:** an open and a closed OBJ are dropped and selected (winding first).
    With one object selected, the booleans are off. With two they are on. `Smooth Union`
    with k 0.5 gives `smooth-union(normalize(winding), mesh)`, k = 0.5, with operands in
    order. The result is selected, the operands are hidden, the step reads
    `smooth-union 3 (normalize on open.obj)`, and the log carries the E-10 line. Undo shows
    the operands again.
  - **`objects_panel`:** Ctrl+click adds rows in order, Ctrl+click on a selected row drops
    it, and a plain click selects one row alone.
- **Docs:**
  - `docs/command_reference/01-viewport.md`: the `Operations` menu row; the Ctrl and
    Shift clicks under *Objects*; the new *Booleans* section; the undo row.
  - `docs/design/04-scene-model.md` § Operations: the fold, the E-10 wrap and the window.
  - `docs/decisions/02-open.md` E-10: an implementation note.
  - The roadmap focus.

## Acceptance criteria

| Criterion | Evidence |
|---|---|
| Each operation's field values match DualC's `dualc_field` output on the same graph; a ctest checks this on sample points | The `operations` ctest gives a max difference of 0 over 216 points for each of the 35 results (7 ops × 5 selections, including three-operand folds, a winding operand, TPMS and lattice operands). The graph is read back through `parseJson`, `dualc_field`'s reader. `dualc_field` itself writes a mesh, not field values, so the comparison is between DualC's `FieldGraph` of the editor's graph and the same builder on the hand-written graph. |
| Undo restores the operands | `operations`: after each of the 35 results, `sameScene(after undo, before)` holds. `main_window` checks the same in the window: the operands are visible again after undo. |
| Booleans on any object, lattices included; non-metric operands as E-10 decided | Lattice and TPMS operands are among the selections. A winding operand of a smooth op gets `normalize`, and a mesh operand does not. |

The plan's criteria were right. The plan file is unchanged.

## Measured (dev box, NV92 / Quadro FX 3700, Mesa 26.0.8; no performance meaning, E-04)

- `operations_test`: 0.45 to 0.53 s wall, 234 checks.
- The `main_window` ctest went from about 60 s to about 66 s with the new block (two mesh
  imports and two compiles).
- Full gate: 19/19 ctests, 117 s.

## What I got wrong on the way

- My first pointer formula for the fold counted one `/in/0` too many for every operand. I
  caught it by reasoning before any build, so no test ever ran with it.
- The `objects_panel` test first restored the single selection with `setCurrentItem(c)`.
  After a Ctrl+click had deselected `c`, `c` was still the current item, so the call did
  nothing. The test now uses a plain click, as a person would.

## Open

- **A viewport click does not collapse a multi-selection** when it picks the object that
  was selected last. `selectObject` only re-selects when the selected id changes. Clicking
  another object, or the background, collapses as before. There is no Ctrl+click in the
  viewport: multi-select happens in the dock only.
- **`normalize` over a winding operand costs gradient samples** on the baked grid at each
  step. Its frame time is not measured (GTX check below).
- **`k` is read once,** when the menu entry is picked. Editing `k` afterwards means
  editing the result's `k` parameter, if the Parameters dock offers it. T3's generated
  panels may replace the spin box.
- **E-10 judged at the operand's top level only.** A smooth boolean nested inside an
  operand, for example a winding under a smooth union an earlier session made by hand
  without a wrap, is not re-wrapped. Results made by the editor never have that shape.

## For the next unit

- T2 can build its fills with `SceneDocument::applyOperation("intersection", {}, {fill,
  volume})` once the fill source is an object. It can also build the graph directly. The
  fill `intersection(onion(normalize(tpms)), volume)` is metric by `isMetricGraph` when the
  volume is, so a later smooth boolean on it does not wrap it. An imported `winding`
  volume makes the fill non-metric, so a later smooth boolean wraps the whole fill.
- `MainWindow::applyBoolean` and the `Operations` menu are where T3's generated panels
  attach. The actions carry the op token in `QAction::data()`.

## Human check

A person should confirm in the window, on any machine:
1. Open or add a sphere and a box (or drop two meshes). Ctrl+click both in the Objects
   dock. `Operations ▸ Difference` cuts the second from the first. Swap the click order
   and the cut swaps too.
2. `Smooth Union` with `Blend k` 0.25 and then 2 mm shows a fillet that grows. Undo shows
   the two operands again.
3. Drop an open mesh (`[winding]`) and smooth-union it with a sphere. The `Edit` menu's
   Undo entry reads `Undo smooth-union N (normalize on <file>)`, and the blend looks
   smooth, with no bulge or hole at the seam.

## GTX check owed

On the GTX 1050 Ti laptop, at this commit, in a Release build:
1. Drop a closed sphere mesh and an open one (any `[winding]` soup), then smooth-union them
   (k 0.25 mm). Read the status bar's `frame` and `preview` ms for the result, against the
   two operands shown side by side before the operation.
2. Record both in `docs/roadmap/02-measurements.md` under a new *Booleans (P05 T1)* line.
   If the `normalize(winding)` result settles far above the frame target of
   `docs/roadmap/03-go-no-go.md`, note it there: wrapping in `normalize` costs gradient
   samples per step on the baked grid.
