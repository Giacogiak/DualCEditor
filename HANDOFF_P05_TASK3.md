# Handoff — P05 T3: operation panels generated from DualC's registries

Date: 2026-10-13. Plan: `plan/60_P05_operations.md` § T3. DualC pin: `2fcb6ee` (unchanged).

**Status: done; P05's exit criteria hold, with T1's and T2's GTX checks still owed (queued,
below).** The `Operations` menu's boolean entries, their `Blend k`, and both fill dialogs'
`Type` and length fields are now generated from one catalogue built from DualC's
`example_common` registries. No widget is written per op. `python scripts/check.py`
exits 0 on the dev box (NV92), with 19/19 ctests.

## What changed and why

- **`src/operation_catalogue.{h,cpp}` (new, GL-free):**
  - `operationCatalogue()` holds one `OpEntry` per op, in DualC's order: the token,
    the family (`Boolean`, `Tpms`, `Strut`), DualC's blurb, `smooth`, and the
    parameters (`OpParam`: key, label, tooltip, default). The ops are:
    - **booleans:** DualC's `BoolOp` values. DualC has no list of them, so the code walks
      `static_cast<BoolOp>(i)` from 0 and stops when `boolOpName` no longer parses back to
      the same value (`boolOpName` falls back to `"union"` past the last value).
      `boolOpIsSmooth` gives `smooth`;
    - **TPMS:** `tpmsKinds()`;
    - **crystals:** `strutKinds()`.
  - **Parameters are per family, not per op.** DualC's registries carry a name and a blurb
    but no parameter list; its field-graph builder reads the params by name
    (`field_graph.cpp`). So the editor writes each family's list once:
    - smooth boolean: `k` (0.25, DualC's default);
    - TPMS: `wavelength`, `thickness`;
    - strut: `wavelength`, `radius`, `nodeRadius`.

    `operationParams(op, box)` gives an op's list at the defaults for a box, through
    T2's `defaultFill`.
  - `operationsOf(family)`, `findOperation(op)`, and `operationTitle(op)` (the menu text:
    `smooth-union` gives `Smooth Union`).
- **`src/operation_panel.{h,cpp}` (new):**
  - `paramSpinBox(param, prefix)` is the one spin box every panel uses: mm suffix, 4
    decimals, range 0.0001–100000, the tooltip, named prefix + the key capitalised
    (`fillNodeRadius`, `smoothK`).
  - `OperationPanel` is a `Type` combo over a family's entries (with the blurb as the
    tooltip) and one row per parameter. Picking an op with different parameter keys
    rebuilds the rows; one with the same keys keeps the values.
- **`FillDialog`** is now an `OperationPanel` plus the skin row (the skin's spin box is a
  `paramSpinBox` too). Its API (`spec`, `setKind`, `setSkin`) and the widget names are
  unchanged.
- **`MainWindow`'s `Operations` menu:**
  - It makes one entry per catalogue boolean: the sharp ones, then a separator, then the
    smooth ones. Each has the title-cased token as text and the token in `data()`.
  - `Blend k` is the generated `paramSpinBox` of the smooth booleans' `k`.
  - The hand-written `&` mnemonics are gone, because the text is generated.
- **`isSmoothBooleanOp`** now uses DualC's `boolOpIsSmooth`, not the editor's three-token
  list. A new smooth boolean in DualC therefore also gets its `normalize` wrap (E-10) and
  its `k`, with no editor change.
- **Tests:**
  - **`operations` (GL-free), 420 checks (383 before), about 0.57 s.** The new checks:
    - the booleans are the seven DualC parses;
    - the TPMS are `tpmsKinds()` (6) and the crystals are `strutKinds()` (4), in order;
    - smooth booleans have `k` = 0.25 and sharp ones have no parameter;
    - the titles are right;
    - an unknown op has no entry;
    - fill parameters start at `defaultFill`;
    - **every one of the 27 parameters is read by DualC.** Raising it changes the field
      DualC's `FieldGraph` builds, at the 216 sample points. This shows each key is one
      DualC's builder reads.
    - **The command-reference page names every catalogue op in backticks.** The ctest
      passes it `docs/command_reference/01-viewport.md`.
  - **`main_window`:**
    - for each of the 10 TPMS and crystals, `FillDialog::setKind` gives `spec().kind`
      equal to the op, and one `fill<Key>` spin box per parameter, plus the skin, at its
      default, in mm;
    - the menu's boolean entries equal the catalogue's booleans;
    - `smoothK` exists, in mm.
- **Docs:**
  - `docs/command_reference/01-viewport.md`:
    - a new *Operation list* section: all 17 ops with family, menu location, parameters
      and defaults, and how they are generated;
    - the `Operations` row points to it;
    - the `Blend k` range is now 0.0001–100000.
  - `docs/command_reference/README.md`'s index line.
  - `docs/design/04-scene-model.md` § Operations: the catalogue/panel bullet.
  - The roadmap focus: P05 done, now P06.

## Acceptance criteria (T3)

| Criterion | Evidence |
|---|---|
| Every boolean, TPMS and strut op in DualC's vocabulary gets its panel without hand-written widget code | `operations`: the catalogue equals DualC's boolean, `tpmsKinds()` and `strutKinds()` sets, in order, and each parameter is one DualC reads. `main_window`: all 10 fill ops get their generated fields, and all 7 booleans get their menu entry and `k`. No widget code names an op; the fields come from the family parameter lists. |
| The command-reference page lists the operations | `01-viewport.md` § *Operation list*. `ctest -R operations` fails if a catalogue op is missing from the page. |

**One reading is recorded here; the plan file is unchanged.** "A new DualC op appears without
editor changes" holds for a new op of an existing family: a boolean, a TPMS or a crystal.
A new *family*, or a new parameter on an existing one, needs the family list in
`operation_catalogue.cpp`, because DualC's registries have no parameter schema. Closing
that gap would mean a parameter registry in DualC (rule 5: a PR on DualC; see *Open*).

## Exit criteria (P05), one bullet at a time

`- Tasks T1 to T3 are met. python scripts/check.py is green.`

- **T1** (`HANDOFF_P05_TASK1.md`):
  - Field equality: `operations` checks every boolean against DualC's field on the
    hand-written graph, and it passes in this run.
  - Undo: same test, passing.
  - Its GTX timing of a `normalize(winding)` smooth union is owed and queued (below). It is
    a measurement, not one of T1's acceptance criteria.
- **T2** (`HANDOFF_P05_TASK2.md`):
  - Renders, a boolean on the fill works, mm and real defaults: `operations` and
    `main_window` pass in this run.
  - The human criterion (the octet fill timed on the GTX, and E-28's trigger) is owed and
    queued (gtx-queue), as the run's rules say: batched, not blocking. E-28 stays
    undecided until it runs.
- **T3:** the table above.
- **Gate:** `python scripts/check.py` → `[gate] all checks passed`, 19/19 ctests, 124.6 s
  on the dev box.

## Measured (dev box, NV92 / Quadro FX 3700, Mesa 26.0.8; no performance meaning, E-04)

- `operations_test`: 0.57 s, 420 checks.
- `main_window` ctest: 76.6 s (T2: 76–78 s). The panel checks add no compile.
- Full gate: 19/19, 124.6 s (T2: 124.5 s).
- Nothing here touches rendering, so no GTX check is owed for T3 itself.

## What I got wrong on the way

- **The first `main_window` check** compared `findChildren().size()` (a `qsizetype`) with a
  `size_t`. That gave a `-Wsign-compare` warning, which would fail
  `-DDUALCEDITOR_WERROR=ON`. I cast it before committing.
- **The doc check passed before I wrote the *Operation list* table.** The *Booleans* table
  and the *Fills* `Type` row already named all 17 tokens in backticks. The check
  therefore guards against a *new* DualC op that the page does not name. The table is
  there because the plan asks for a list.

## Open

- **There is no parameter registry in DualC.** The per-family lists in
  `operation_catalogue.cpp` mirror what `field_graph.cpp` reads (`k`, `wavelength`,
  `radius`, `nodeRadius`). If DualC renamed one, test 10 would catch it, but the editor
  would not follow on its own. A `params` field on `TpmsKind`, `StrutKind` and the
  booleans in DualC would remove the editor's lists. That is a question for the owner and
  a DualC PR (rule 5), not blocking.
- **The k default (0.25) is also a mirror** of `field_graph.cpp`'s `numParam(n, "k", 0.25)`.
- **The menu lost its explicit mnemonics.** Qt's automatic mnemonics apply on some
  platforms only. If the owner wants `Alt` shortcuts, an accelerator can be derived from
  the title, avoiding letters already used.
- Carried from T2: the defaults read the padded trace box; editing the wavelength does not
  rescale `t` or the radii; no E-28 verdict until the GTX check runs.

## For the next unit (P06)

- `operationCatalogue()` is the one list of operations. An export panel offering DualC's
  ops, or a `--list` style dump, should read it.
- `paramSpinBox` is the shared mm field: use it for any new length field (export cell size,
  for example) so units and ranges stay consistent.

## Human check

On any machine, in the window:
1. `Operations` lists `Union`, `Intersection`, `Difference`, `Xor`, a separator,
   `Smooth Union`, `Smooth Intersection`, `Smooth Difference`, then the `Blend k` row at
   0.2500 mm, then the two fill entries, as before T3. Check that the texts read well
   without the old underlined mnemonic letters.
2. Drop a closed mesh, `Fill with TPMS…`: `Type` lists the six TPMS (hover shows DualC's
   blurb), then `Wavelength` and `Wall thickness t` in mm, then `Keep the skin`. Switch the
   type: the values stay. `Fill with Lattice…`: `Wavelength`, `Radius`, `Node radius`, then
   the skin. `OK` fills as in T2.
3. A smooth union with `Blend k` changed to 1 mm visibly blends wider than at 0.25.

## GTX check owed

Nothing new for T3. P05's owed checks, already queued, still stand. Run them at this commit:
- **T1** (`HANDOFF_P05_TASK1.md` § GTX check owed): the timing of a smooth union with a
  `normalize(winding)` operand.
- **T2** (`HANDOFF_P05_TASK2.md` § GTX check owed):
  - `python scripts/measure_viewport.py --fills`: the octet fill at 0.35 against P00.5 ·
    tapered octet (P12) · 0.35 = 80.6 GPU / 83.3 wall ms moving, 385.3 / 389.5 settled;
  - whether a drag lags at its start;
  - E-28's trigger, recorded in `docs/roadmap/03-go-no-go.md` item 4 or 5;
  - the octet fill renders as a clean truss.
