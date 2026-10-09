# P05 — Booleans, lattice fills, operation panels

Waits on **E-10** (smooth booleans on non-metric inputs). Each operation is a panel that
takes the selected objects and produces a new object.

### T1. Booleans and smooth booleans

Union, intersection, difference and xor, plus the smooth variants with `k`, applied to the
selected objects in selection order. They work on any object, lattices included. Treat
non-metric operands as E-10 decided.

#### Acceptance criteria
- Each operation gives an object whose field values match DualC's `dualc_field` output on
  the same graph. A ctest checks this on sample points.
- Undo restores the operands.

### T2. TPMS fill and strut-lattice fill

"Fill with TPMS": pick a type and set wavelength and wall thickness. It builds
`intersection(onion(normalize(tpms)), volume)`.

"Fill with lattice": pick sc, bcc, fcc or octet, and set radius, node radius and
wavelength. It builds `intersection(lattice, volume)`.

Both offer an optional shell that keeps the volume's skin: a union with an `onion` of the
volume.

#### Acceptance criteria
- Filling an imported mesh with a gyroid, and separately with an octet lattice, renders
  correctly. A boolean on the filled result works.
- The panels show the units (mm) and the real defaults.

### T3. Operation panels generated from DualC's registries

Generate the parameter widgets from DualC's op registries in `example_common`, so that a
new DualC op appears without editor changes.

#### Acceptance criteria
- Every boolean, TPMS and strut op in DualC's vocabulary gets its panel without
  hand-written widget code.
- The command-reference page lists the operations.

## Exit criteria
- Tasks T1 to T3 are met. `python scripts/check.py` is green.
