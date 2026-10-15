# P05 — Booleans, lattice fills, operation panels

**DONE (2026-10-13).** Waited on E-10 (smooth booleans on non-metric inputs; decided
2026-10-13 by delegation: wrap in `normalize` automatically). Each operation takes the
selected objects and produces a new object. Design: `../../design/04-scene-model.md`
§ Operations. Usage: `../../command_reference/01-viewport.md` § Booleans, § Fills,
§ Operation list, `--fills`. GTX checks owed: `../04-gtx-batch-2026-10-13.md` § 6 and § 7
(E-28's trigger). Development-box conditions: `README.md`.

## T1. Booleans and smooth booleans

*Planned:* union, intersection, difference and xor, plus the smooth variants with `k`,
applied to the selected objects in selection order; on any object, lattices included;
non-metric operands as E-10 decided. Acceptance: each operation's field values match
DualC's `dualc_field` output on the same graph, a ctest checking sample points; undo
restores the operands.

**DONE (2026-10-13, commit d66da8a).** Delivered: in `operationGraph`
(`src/scene_document.cpp`), **the fold**: DualC's booleans take exactly two children
(`requireChildren(n, 2)`), so over more than two operands a boolean folds left,
`op(op(a, b), c)`, every node carrying the same params; the pointers stay structural (three
operands: `/root/in/0/in/0`, `/root/in/0/in/1`, `/root/in/1`; two operands unchanged, so
P03's tests and saved scenes hold). Before this, three operands built a three-input
`union` DualC rejects. **The E-10 wrap**: when `op` is a smooth boolean and
`isMetricGraph(operand)` is false, the operand goes under `normalize`, outermost, above
`transform` and `scale`; `normalized` returns the wrapped indices. `isMetricGraph` is false
when the graph holds, with no `normalize` above it, a `winding`, a TPMS token from
`dce::tpmsKinds()`, or a `twist`, `bend` or `displace` (step scale below 1 in DualC's
`nodeStepScale`, as TPMS and winding). A `mesh` counts as metric: a signed distance near
the surface, the only region a blend of width `k` reads, and wrapping it would add
`normalize`'s gradient samples to the mesh grid's cost for nothing; strut lattices are
distances. `isBooleanOp`, `isSmoothBooleanOp` (through DualC's `parseBoolOp`).
`applyOperation`'s step text is `"<name> (normalize on a, b)"` when a wrap happened: the
history E-10's option asks for. `ObjectsPanel`: `ExtendedSelection` with `order_` holding
the selection order (`syncOrder` drops deselected rows and appends new ones in row order);
`selectedIds()` in that order; `selectedId()` is the last selected, which the gizmo, the
Transform dock and the highlight follow; `selectInOrder`, `select` as `ClearAndSelect`,
`setObjects` keeps the ordered selection, `selectionOrderChanged`. `MainWindow`: the
`Operations` menu, actions carrying the op token in `data()`, `applyBoolean(op)`,
`smoothK()` / `setSmoothK()` (mm, default 0.25, DualC's default), enabled with at least two
selected objects in both the document and the scene on screen; `selectAfter_` selects the
result when its scene commits; the log and status line read like
`smooth-union of open.obj, closed.obj, k 0.5 mm; normalize on open.obj, not distance fields (E-10)`.
Tests: `operations` (GL-free; five objects: a sphere, a box moved and scaled 1.5, an
imported open box as `winding`, a clipped raw gyroid, a clipped octet lattice; all 7 ops
over 5 selections, (sphere, box), (box, sphere), (sphere, winding), (octet, gyroid),
(sphere, octet, box)); `main_window` (an open and a closed OBJ dropped and selected,
winding first; booleans off with one selected, on with two; `Smooth Union` k 0.5 gives
`smooth-union(normalize(winding), mesh)` with operands in order, the result selected, the
operands hidden, the step `smooth-union 3 (normalize on open.obj)`, the E-10 log line; undo
shows the operands); `objects_panel` (Ctrl+click adds rows in order, Ctrl+click on a
selected row drops it, a plain click selects one). 19/19 ctests.
Verified: for each of the 35 results, the graph dumped and re-read as `dualc_field` reads
one (`dumpJson`, `parseJson`), built by DualC, equals at 216 points the field of the graph
written by hand in DualC's shorthand (operands posed, folded left, non-metric ones under
`normalize` for the smooth ops): max difference 0; the step text names the normalize
exactly when one was added; the operands are hidden; undo gives `sameScene` and redo the
same graph; sharp `union`, `intersection`, `difference` equal `min`, `max`, `max(a, −b)` of
the operands' fields; smooth union and difference over (sphere, winding) and (octet,
gyroid) lower to GLSL. (`dualc_field` itself writes a mesh, not field values, so the
comparison is DualC's `FieldGraph` of the editor's graph against the same builder on the
hand-written graph.) Development box: `operations_test` 0.45–0.53 s, 234 checks;
`main_window` from about 60 s to 66 s (two imports, two compiles); full gate 117 s.
- *Wrong on the way:* the first pointer formula for the fold counted one `/in/0` too many
  per operand, caught by reasoning before any build. The `objects_panel` test restored a
  single selection with `setCurrentItem(c)` after a Ctrl+click had deselected `c`, which
  was still current, so the call did nothing; a plain click now.
- *Open:* a viewport click does not collapse a multi-selection when it picks the object
  selected last (`selectObject` re-selects only when the id changes); clicking another
  object or the background collapses; no Ctrl+click in the viewport. `normalize` over a
  winding operand costs gradient samples on the baked grid at each step, unmeasured (§ 6
  of the batch). `k` is read once, when the entry is picked; editing it afterwards means
  editing the result's `k` in the Parameters dock. E-10 is judged at the operand's top
  level: a smooth boolean nested inside an operand made by hand without a wrap is not
  re-wrapped (results made by the editor never have that shape).
- **Window check owed, any machine** (`../04-gtx-batch-2026-10-13.md` § 10): Ctrl+click a
  sphere and a box, `Difference` cuts the second from the first and swapping the click
  order swaps the cut; `Smooth Union` at `Blend k` 0.25 then 2 mm shows a fillet that grows,
  undo shows both operands; an open mesh (`[winding]`) smooth-unioned with a sphere reads
  `Undo smooth-union N (normalize on <file>)` in the Edit menu and blends smoothly with no
  bulge or hole at the seam.

## T2. TPMS fill and strut-lattice fill

*Planned:* "Fill with TPMS" (type, wavelength, wall thickness) builds
`intersection(onion(normalize(tpms)), volume)`; "Fill with lattice" (sc, bcc, fcc or
octet; radius, node radius, wavelength) builds `intersection(lattice, volume)`; both offer
an optional shell keeping the volume's skin, a union with an `onion` of the volume.
Acceptance: filling an imported mesh with a gyroid and with an octet renders correctly and
a boolean on the result works; the panels show mm and the real defaults; the owner times
the octet fill of an imported mesh on the GTX at 0.35 against P00.5 · tapered octet (P12)
· 0.35 and says whether it still lags at the start of a drag, deciding E-28's trigger
(`../03-go-no-go.md` item 4).

**DONE (2026-10-13, commit 623b6fa), GTX check owed; E-28 undecided until it runs.**
Delivered: `FillSpec` (kind, wavelength, centre in world, TPMS `thickness`, strut `radius`
and `nodeRadius`, `skin`, lengths in mm); `fillSource(spec)` gives
`onion(normalize(<tpms>(center, wavelength)), thickness)` or
`<strut>(center, wavelength, radius, nodeRadius)`; `fillGraph(spec, volume, edits)` gives
`intersection(source, volume)`, built from `operationGraph` calls over stand-in objects at
the identity, so the volume is posed as a boolean's operand is, pointers stay structural and
the volume's edits are re-keyed (no second posing code). **The skin**: the graph is
`union(clip, onion(offset(volume, r=-s/2), thickness=s/2))`, the volume's inside within
`s` of its surface; an onion of the volume itself would straddle the surface and grow the
part by `s/2`, changing a printed part's size; a `winding` volume goes under `normalize`
inside the skin only, since an onion reads distances. `defaultFill(kind, box)`: the
wavelength is the box diagonal / 10 as in DualC's `dualc_lattice`; the centre the box
centre; the TPMS `t` wavelength / 5 (the ratio of DualC's `mesh_lattice.json`, 0.1 at 0.5);
both strut radii wavelength / 10 (DualC's builder default, 0.1 at 1); each rounded to three
significant digits so the dialog shows exactly what the fill uses. `isTpmsKind`,
`isStrutKind`. `SceneDocument::applyFill` makes `<kind> fill <id>` with
`operands = {volume}`, placed after the volume, which is hidden; the undo command is T1's
`ApplyOperation`. `FillDialog` (`src/fill_dialog`): a `Type` combo over DualC's kinds with
DualC's blurb as tooltip, then `Wavelength` and `Wall thickness t` or `Radius` and `Node
radius`, then `Keep the skin` and its thickness, every spin box with ` mm` and a tooltip
saying where its default comes from. `MainWindow`: two entries under a separator in
`Operations` (`fill-tpms`, `fill-lattice` in `data()`), enabled when the selected object is
in the document and on screen; `applyFill` logs the parameters and selects the result;
`worldBounds(id)` is the posed trace box the defaults read.
`scripts/measure_viewport.py --fills`: one imported UV sphere filled with an octet at the
dialog's defaults (wavelength 0.152, radii 0.0152) beside the P00 tapered octet, at 0.35
and 1.0, rows `P05.2`. Tests: `operations` (defaults from a ±10 mm box: wavelength 3.46,
radii 0.346, t 0.692, centred; an unknown kind or volume refused; four fills, gyroid,
schwarz-p, octet and bcc tapered 0.5/0.9, of three volumes, each with and without a 2 mm
skin, 24 cases; the skin on the radius-10 sphere −1 mm at depth 1, solid at 1.5, empty at
2.5 and outside); `main_window` (a dropped closed OBJ as the volume, both entries enabled,
both dialogs at `defaultFill` of the window's world box with wavelength 0.381 mm, t
0.0762 mm, radius 0.0381 mm for the ±1 mm cube, every spin box in mm, a ticked skin taken;
a gyroid fill then an octet fill each come on screen with the expected graph, selected,
hiding the mesh, changing the image; a `union` of the two fills; three undos show the
mesh). 19/19 ctests.
Verified: each of the 24 fills, read back through `dumpJson` and `parseJson`, equals the
hand-written shorthand at 216 points (max difference 0); the fill is metric exactly when
the volume is; the volume is hidden; a `difference` with a sphere equals `max(fill,
−sphere)`; undo gives `sameScene`, redo the same graph. On the development box
`--snapshot` of a 20 mm cube filled at the defaults shows the gyroid fill correctly (walls
cut at the faces); the octet fill shows NV92's flat yellow and magenta, the nouveau artefact
P00 T5 recorded, so "renders correctly" for the octet is the GTX check. Development box:
`operations_test` 0.54 s, 383 checks; `main_window` 76–78 s (one drop, four compiles); gate
124.5 s; `--fills --frames 3 --size 320x200` ran the fill scene, then the tapered octet
failed with nouveau's `nv50_program_upload_code: shader too large`, a dev-box limit.
Readings recorded (the plan's criteria stand): the skin is an onion of the volume shrunk
inward, which is still "a union with an onion of the volume"; "real defaults" means the
values the fill uses, derived from the volume as `dualc_lattice` derives them (DualC's bare
builder default, wavelength 1 mm, would be meaningless on a 100 mm mesh).
- *Wrong on the way:* the first default rounding, `round(v / unit) * unit`, gave
  3.4600000000000004; now `round(v * scale) / scale` (a test caught it). The skin probe's
  first lattice was an `sc` cell centred on the origin, whose struts cross at its centre and
  ran through the probes; centred 50 mm away now (a test caught it). A first test draft
  applied a fill inside a check expression and left it in the document; removed before it
  ran. A `.fld` naming `tests/data/meshes/cube.obj` fails in DualC's mesh reader
  (`basic_string::substr`; the file has a comment line and an `o` line, which the editor's
  reader accepts); a bare OBJ was used instead and it was not investigated (rule 5;
  `../06-dualc-questions.md`).
- *Open:* the defaults read the trace box, padded 5% a side, so the wavelength is 1.1 ×
  `dualc_lattice`'s (0.381 instead of 0.346 for a ±1 cube); unpadding exactly is not
  possible for a rotated volume; harmless, stated in the doc. Editing the wavelength in the
  dialog does not rescale `t` or the radii. The fill's parameters can be edited afterwards
  in the Parameters dock as uniforms; the skin's offset and thickness are separate
  parameters there. A fill of a `winding` volume is not metric, so a later smooth boolean
  wraps the whole fill in `normalize`. **No E-28 verdict until the GTX check** (§ 7 of the
  batch).
- **Window check owed, any machine** (`../04-gtx-batch-2026-10-13.md` § 10): `Fill with
  TPMS…` on a dropped closed mesh shows `gyroid`, a wavelength about a tenth of the mesh's
  size, `t` a fifth of that, all in mm, and `OK` shows a gyroid-walled solid in the mesh's
  shape with the mesh hidden; undo, then `Fill with Lattice…` with `octet` gives an octet
  truss, and with `Keep the skin` a closed outer surface with the lattice visible through a
  section; Ctrl+click the fill and another object, `Difference` shows the cut, undo shows
  the fill.

## T3. Operation panels generated from DualC's registries

*Planned:* generate the parameter widgets from DualC's op registries in `example_common`,
so a new DualC op appears without editor changes. Acceptance: every boolean, TPMS and strut
op in DualC's vocabulary gets its panel without hand-written widget code; the command
reference lists the operations.

**DONE (2026-10-13, commit bb24e97).** Delivered: `src/operation_catalogue` (GL-free):
`operationCatalogue()` holds one `OpEntry` per op in DualC's order (token, family
`Boolean` / `Tpms` / `Strut`, DualC's blurb, `smooth`, parameters as `OpParam`: key, label,
tooltip, default). Booleans are DualC's `BoolOp` values: DualC has no list, so the code
walks `static_cast<BoolOp>(i)` from 0 and stops when `boolOpName` no longer parses back to
the same value (`boolOpName` falls back to `"union"` past the last); `boolOpIsSmooth` gives
`smooth`. TPMS from `tpmsKinds()`, crystals from `strutKinds()`. **Parameters are per
family, not per op**: DualC's registries carry a name and a blurb but no parameter list
(its builder reads params by name in `field_graph.cpp`), so the editor writes each family's
list once: smooth boolean `k` (0.25); TPMS `wavelength`, `thickness`; strut `wavelength`,
`radius`, `nodeRadius`. `operationParams(op, box)` through T2's `defaultFill`;
`operationsOf(family)`, `findOperation(op)`, `operationTitle(op)` (`smooth-union` →
`Smooth Union`). `src/operation_panel`: `paramSpinBox(param, prefix)` is the one spin box
every panel uses (mm suffix, 4 decimals, range 0.0001–100000, the tooltip, named prefix +
capitalised key, e.g. `fillNodeRadius`, `smoothK`); `OperationPanel` is a `Type` combo over
a family's entries plus one row per parameter; an op with different parameter keys
rebuilds the rows, one with the same keys keeps the values. `FillDialog` is now an
`OperationPanel` plus the skin row, its API and widget names unchanged. The `Operations`
menu makes one entry per catalogue boolean (sharp, a separator, smooth), `Blend k` is the
generated spin box of the smooth booleans' `k`, and the hand-written `&` mnemonics are
gone because the text is generated. `isSmoothBooleanOp` now uses DualC's `boolOpIsSmooth`,
so a new smooth boolean in DualC gets its `normalize` wrap and its `k` with no editor
change. Tests: `operations` 420 checks (the booleans are the seven DualC parses; the TPMS
are `tpmsKinds()` (6) and the crystals `strutKinds()` (4), in order; smooth booleans have
`k` = 0.25 and sharp ones no parameter; the titles; an unknown op has no entry; fill
parameters start at `defaultFill`; **every one of the 27 parameters is read by DualC**:
raising it changes the field at the 216 points; **the command-reference page names every
catalogue op in backticks**, the ctest reading `docs/command_reference/01-viewport.md`);
`main_window` (for each of the 10 TPMS and crystals `setKind` gives `spec().kind` equal to
the op and one `fill<Key>` spin box per parameter plus the skin at its default in mm; the
menu's booleans equal the catalogue's; `smoothK` exists in mm). 19/19 ctests
(`operations_test` 0.57 s; `main_window` 76.6 s; gate 124.6 s). Nothing touches rendering,
so no GTX check for T3.
Reading recorded (the plan file was unchanged): "a new DualC op appears without editor
changes" holds for a new op of an existing family; a new *family*, or a new parameter on an
existing one, needs the family list in `operation_catalogue.cpp`, because DualC's
registries have no parameter schema (`../06-dualc-questions.md`).
- *Wrong on the way:* the first `main_window` check compared `findChildren().size()` with a
  `size_t` (`-Wsign-compare`, which fails `-DDUALCEDITOR_WERROR=ON`); cast. The doc check
  passed before the *Operation list* table was written, because the *Booleans* table and
  the `Type` row already named all 17 tokens; the check guards against a *new* DualC op
  the page does not name, and the table is there because the plan asks for a list.
- *Open:* the per-family lists and the `k` default (0.25, `numParam(n, "k", 0.25)`) mirror
  `field_graph.cpp`; if DualC renamed one, the ctest would catch it but the editor would
  not follow on its own. The menu lost its explicit mnemonics; Qt's automatic ones apply on
  some platforms only, and an accelerator could be derived from the title if `Alt`
  shortcuts are wanted.
- **Window check owed, any machine** (`../04-gtx-batch-2026-10-13.md` § 10): `Operations`
  lists `Union`, `Intersection`, `Difference`, `Xor`, a separator, `Smooth Union`, `Smooth
  Intersection`, `Smooth Difference`, the `Blend k` row at 0.2500 mm, then the two fill
  entries, reading well without underlined mnemonics; `Fill with TPMS…` lists the six TPMS
  (hover shows DualC's blurb), then `Wavelength`, `Wall thickness t`, `Keep the skin`, and
  switching the type keeps the values; `Fill with Lattice…` lists `Wavelength`, `Radius`,
  `Node radius`, the skin; a smooth union at `Blend k` 1 mm visibly blends wider than at
  0.25.

## Exit criteria

T1 to T3 met as above (T1's GTX timing of a `normalize(winding)` smooth union and T2's
octet timing with E-28's trigger are measurements, queued, not acceptance criteria);
`python scripts/check.py` green (19/19 ctests). For a later phase: `operationCatalogue()`
is the one list of operations, and `paramSpinBox` the shared mm field for any new length.

---
← Back to the [Plan record](README.md) · [Roadmap index](../README.md).
