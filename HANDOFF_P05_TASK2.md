# Handoff — P05 T2: TPMS fill and strut-lattice fill

Date: 2026-10-13. Plan: `plan/60_P05_operations.md` § T2. DualC pin: `2fcb6ee` (unchanged).

**Status: done, human check and GTX check owed.** `Operations ▸ Fill with TPMS…` and
`Fill with Lattice…` act on the selected object, the volume. Each opens a dialog with a
type, the lengths in mm at their real defaults, and an optional skin. `OK` makes one new
object, hides the volume and is one undo step. `python scripts/check.py` exits 0 on the
dev box (NV92), with 19/19 ctests.

**E-28's trigger (`docs/roadmap/03-go-no-go.md`, item 4): not decided yet.** It needs the
owner's GTX timing below. The dev box cannot measure it (E-04).

## What changed and why

- **`src/scene_document.{h,cpp}`:**
  - **`FillSpec`** holds the kind (a DualC TPMS token or strut token), the wavelength,
    the centre (world), the TPMS `thickness`, the strut `radius` and `nodeRadius`, and
    `skin` (0 for none). Lengths are in mm.
  - **`fillSource(spec)`** gives `onion(normalize(<tpms>(center, wavelength)), thickness)`
    or `<strut>(center, wavelength, radius, nodeRadius)`, as the plan asks.
  - **`fillGraph(spec, volume, edits)`** gives `intersection(source, volume)`, the source
    first, as the plan writes it. It is built from `operationGraph` calls over stand-in
    objects at the identity. That way the volume is posed (`transform`, `scale`) the way
    a boolean's operand is, the pointers stay structural, and the volume's parameter edits
    are re-keyed into the result. No second posing code.
  - **The skin.** The plan says "a union with an onion of the volume". The graph is
    `union(clip, onion(offset(volume, r=-s/2), thickness=s/2))`, which is the volume's
    inside within `s` of its surface. An onion of the volume itself would straddle the
    surface and grow the part by `s/2`, which changes a printed part's size. A `winding`
    volume goes under `normalize` inside the skin only, because an onion reads distances.
    The clip does not need the wrap.
  - **`defaultFill(kind, box)`** computes the dialogs' defaults:
    - the wavelength is the box diagonal / 10, as in DualC's `dualc_lattice`;
    - the centre is the box centre;
    - the TPMS `t` is wavelength / 5, the ratio of DualC's `mesh_lattice.json`
      (0.1 at 0.5);
    - both strut radii are wavelength / 10, the ratio of DualC's builder default
      (0.1 at 1).

    Each value is rounded to three significant digits, so the dialog shows exactly what
    the fill uses.
  - **`isTpmsKind` and `isStrutKind`** check a token against DualC's `tpmsKinds()` and
    `strutKinds()`.
  - **`SceneDocument::applyFill`** makes the new object `<kind> fill <id>`, with
    `operands = {volume}`. It is placed after the volume, and the volume is hidden if it
    was shown. The undo command is T1's `ApplyOperation`. An unknown kind or volume
    returns 0.
- **`src/fill_dialog.{h,cpp}` (new):**
  - `FillDialog(Family, volume name, world box)` has a `Type` combo listing DualC's kinds,
    with DualC's blurb as each item's tooltip.
  - `Wavelength`, then `Wall thickness t` (TPMS) or `Radius` and `Node radius` (lattice),
    then `Keep the skin` with its thickness. Every spin box has the ` mm` suffix and a
    tooltip that says where its default comes from.
  - `spec()` returns the `FillSpec`; `setKind` and `setSkin` exist for tests.
- **`MainWindow`:**
  - Two entries under a separator in the `Operations` menu, with `data()` set to
    `fill-tpms` and `fill-lattice`. They are enabled when the selected object is both in
    the document and on screen.
  - `fillDialog(lattice)` opens the dialog (modal).
  - `applyFill(spec, volume)` applies the fill, logs the parameters and selects the result
    once it is on screen.
  - `worldBounds(id)` is the object's posed trace box, which the defaults read.
- **`scripts/measure_viewport.py --fills`** runs one imported UV sphere, filled with an
  octet at the dialog's defaults (wavelength 0.152, radii 0.0152), beside the P00 tapered
  octet, at 0.35 and 1.0. Rows are labelled `P05.2`. This is the GTX check's tool.
- **Tests:**
  - **`operations` (GL-free), 383 checks, about 0.54 s:**
    - The defaults: from a ±10 mm box, wavelength 3.46, radii 0.346, t 0.692, centred.
    - An unknown kind or volume is refused.
    - Four fills (gyroid, schwarz-p, octet, and bcc tapered 0.5/0.9) of three volumes
      (the sphere, the moved and scaled box, the `winding` open box), each with and
      without a 2 mm skin, 24 cases in all. For each case:
      - the field, read back through `dumpJson` and `parseJson`, equals the hand-written
        shorthand at 216 points (max difference 0);
      - the fill is metric exactly when the volume is;
      - the volume is hidden;
      - a `difference` with a sphere on the fill equals `max(fill, -sphere)`;
      - undo gives `sameScene` with the scene before, and redo gives the same graph.
    - The skin, on the radius-10 sphere: -1 mm at depth 1, solid at depth 1.5, empty at
      depth 2.5 and outside.
  - **`main_window`:**
    - A dropped closed OBJ is the volume. Both fill entries are enabled.
    - Both dialogs open at `defaultFill` of the window's world box: wavelength 0.381 mm,
      t 0.0762 mm and radius 0.0381 mm for the ±1 mm test cube. Every spin box is in mm,
      and a ticked skin is taken.
    - A gyroid fill and then an octet fill each come on screen with the expected graph
      shape, are selected, hide the mesh, and change the framebuffer image.
    - A `union` of the two fills comes on screen.
    - Three undos show the mesh again.
- **Docs:**
  - `docs/command_reference/01-viewport.md`: the `Operations` row, the new *Fills* section
    (fields, units, defaults, skin), the undo row and `--fills`.
  - `docs/design/04-scene-model.md` § Operations: the fill bullet.
  - The roadmap focus and `tests/CMakeLists.txt`'s comment.

## Acceptance criteria

| Criterion | Evidence |
|---|---|
| Filling an imported mesh with a gyroid, and separately with an octet lattice, renders correctly | `main_window`: both fills of a dropped OBJ link, come on screen and change the image. `operations`: both fill graphs equal DualC's field on the hand-written graph, over `mesh` and `winding` volumes. On this box, `--snapshot` of a 20 mm cube filled at the defaults shows the gyroid fill correctly (walls cut at the faces). The octet fill shows NV92's flat yellow and magenta, the nouveau artefact P00 T5 recorded ("a nouveau and Quadro matter"). So "renders correctly" for the octet is a human check on the GTX (below). |
| A boolean on the filled result works | `operations`: a difference on each of the 24 fills equals `max(fill, -sphere)`. `main_window`: a union of the gyroid fill and the octet fill comes on screen. |
| The panels show the units (mm) and the real defaults | `main_window`: every spin box of both dialogs has ` mm`. `spec()` at opening equals `defaultFill` of the volume's world box, the values the fill then uses. |
| Human check: the octet fill timed on the GTX at 0.35 against P00.5 · tapered octet (P12) · 0.35, and whether it lags at the start of a drag; the handoff says whether E-28 fired | Owed: *GTX check owed* below. E-28 is undecided until it runs. |

The plan's criteria stand. Two readings are recorded here, and the plan file is unchanged:
- the skin is an onion of the volume shrunk inward, which still makes a union with an
  onion of the volume;
- "real defaults" means the values the fill uses, derived from the volume as DualC's
  `dualc_lattice` derives them. DualC's bare builder defaults (wavelength 1 mm) would be
  meaningless on a 100 mm mesh.

## Measured (dev box, NV92 / Quadro FX 3700, Mesa 26.0.8; no performance meaning, E-04)

- `operations_test`: 0.54 s, 383 checks (234 before).
- `main_window` ctest: about 76–78 s, up from about 66 s, for one mesh drop and four
  compiles (two fills, a union, the undo).
- Full gate: 19/19, 124.5 s.
- `measure_viewport.py --fills --frames 3 --size 320x200` on this box: the fill scene ran.
  The tapered octet scene then failed with nouveau's
  `nv50_program_upload_code: shader too large`, a dev-box limit. No numbers kept.

## What I got wrong on the way

- **The first default rounding** (`round(v / unit) * unit`) gave 3.4600000000000004, not
  3.46. It is now `round(v * scale) / scale`. A test caught it.
- **The skin probe's first lattice** was an `sc` cell centred on the origin. A cell's
  struts cross at its centre, so a strut ran through the probes. The cell is now centred
  50 mm away. A test caught this too.
- **The first operations test draft** applied a fill inside a check expression and left
  it in the document. I removed it before it ran.
- **A `.fld` naming `tests/data/meshes/cube.obj`** fails in DualC's mesh reader
  (`basic_string::substr`). That file has a comment line and an `o` line, which the
  editor's own reader accepts. My snapshot used a bare OBJ instead. I did not
  investigate further: it is DualC's reader, rule 5.

## Open

- **The defaults read the trace box,** which is padded by 5% a side. The wavelength is
  therefore 1.1 × DualC's `dualc_lattice` default: 0.381 instead of 0.346 for a ±1 cube.
  Unpadding exactly is not possible for a rotated volume. This is harmless, and the doc
  states it.
- **Editing the wavelength in the dialog does not rescale `t` or the radii.** They keep
  the defaults computed when the dialog opened.
- **The fill's parameters** (wavelength, thickness, radii, centre) can be edited afterwards
  in the Parameters dock, as uniforms. The skin's offset and thickness are separate
  parameters there, so changing the skin means editing both, or undoing and filling again.
- **A fill of a `winding` volume is not metric,** so a later smooth boolean wraps the whole
  fill in `normalize`, which costs gradient samples (T1's open GTX item).
- **No E-28 verdict** until the GTX check runs.

## For the next unit (T3)

- T3 generates panels from DualC's registries. The fill dialog is the hand-written widget
  code T3 aims to replace or generate. `FillSpec` and `fillGraph` are the graph side and
  stay. The kinds already come from `dce::tpmsKinds()` and `dce::strutKinds()`, so a new
  DualC TPMS or crystal appears in the combo without editor changes. The per-kind
  parameters do not yet come from a registry.
- The menu entries carry `fill-tpms` and `fill-lattice` in `QAction::data()`, next to T1's
  boolean tokens.

## Human check

On any machine, in the window:
1. Drop a closed mesh, select it and pick `Operations ▸ Fill with TPMS…`. The dialog shows
   `gyroid`, a wavelength about a tenth of the mesh's size, `t` a fifth of that, all in mm.
   `OK` shows a gyroid-walled solid in the mesh's shape, and the mesh is hidden.
2. Undo, then `Fill with Lattice…` with `octet`, `OK`: an octet truss in the mesh's shape.
   Tick `Keep the skin` on a second try: the outer surface is closed, with the lattice
   visible through a section (or with the mesh made transparent by hiding).
3. Ctrl+click the fill and another object, then `Operations ▸ Difference`: the cut shows.
   Undo shows the fill again.

## GTX check owed

On the GTX 1050 Ti laptop, at this commit, in a Release build:
1. `python scripts/measure_viewport.py --fills` (add `--prime` on Linux PRIME). It prints
   rows for `octet fill of an imported mesh` and `tapered octet (P12)` at 0.35 and 1.0,
   labelled `P05.2`. Compare the 0.35 moving GPU ms of the fill with the P00.5 row
   `tapered octet (P12) · 0.35`: 80.6 GPU / 83.3 wall moving, 385.3 / 389.5 settled.
   Record the rows in `docs/roadmap/02-measurements.md` under a *Fills (P05 T2)* heading.
2. In the window (`./build/bin/DualCEditor --preview-scale 0.35`), drop any closed mesh,
   `Fill with Lattice…`, octet, defaults, `OK`. Orbit with short drags started from
   rest. Say whether the view lags at the start of a drag (the settle's full frame
   finishing first).
3. **E-28's trigger:** it fires if step 2 lags at the start of a drag. Then E-28 moves
   from `docs/decisions/03-deferred.md` to open, as `03-go-no-go.md` item 5 says. If there
   is no lag, record "E-28 not triggered (P05 T2)" in `03-go-no-go.md` item 4.
4. While there, confirm the octet fill renders as a clean truss. On the dev box it shows
   NV92's flat yellow and magenta.
