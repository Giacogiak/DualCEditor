# DualCEditor — agent entry

DualCEditor is a standalone Qt 6 editor for DualC field graphs. It sphere-traces the field
live, lets you build a scene of objects through operations (booleans, TPMS and strut-lattice
fills, transforms), and exports meshes through DualC. C++17, MIT. Design background:
`docs/roadmap/01-feasibility.md`.

## Build, test, gate

Qt 6.4 or newer must be installed (`find_package`, never fetched). On Ubuntu:
`sudo apt install qt6-base-dev ninja-build`. DualC is fetched at the commit pinned in
`CMakeLists.txt`, unless `-DDUALCEDITOR_DUALC_DIR=/path` names a local tree. DualC in turn
fetches geometry-central.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/bin/DualCEditor
```

`-DDUALCEDITOR_WERROR=ON` makes warnings on our own targets errors.

**The gate is what CI runs.** `.github/workflows/gate.yml` runs `scripts/check.py`. Run it
before every commit:

```bash
python scripts/check.py          # docs index + hygiene + configure/build/ctest
python scripts/check.py --fast   # docs index + hygiene only
git config core.hooksPath scripts/hooks   # once per clone: fast tier on commit
```

## Rules

1. **Nothing stays in the chat.** Every fix, measurement, decision or rejected approach is
   written to its owner (the table in `docs/README.md`) before the session ends.
2. **Every folder under `docs/` and `plan/` has a README naming every file in it.** The
   gate enforces this.
3. **Ids are permanent.** Decisions are `E-NN`, phases `Pnn`, tasks `Tn`. Nothing is
   renumbered or reused. A DualC decision is cited as `DualC D-NN`.
4. **The command reference grows with the UI.** A flag, key or panel that changes updates
   `docs/command_reference/` in the same session.
5. **DualC is not edited from here.** The editor links DualC's host-side libraries at a
   pinned commit (E-06). A needed DualC change is a question for the owner and a PR on
   DualC.
6. **`main` is reached by pull request.**

## Constraints

- **Scope (E-05, DualC D-04).** Mesh I/O, scene, transforms and UI belong to the editor.
- **Licences (E-01).** Vendored code must be MIT, BSD or public domain. Qt (LGPL) is linked
  dynamically, never vendored.
- **Reference GPU (E-04).** GeForce GTX 1050 Ti 4 GB, on the owner's laptop. This
  development box (Quadro FX 3700) cannot measure performance, so measurements are human
  checks.
- **Keyboard.** The owner uses Belgian AZERTY. Bind keys by meaning, not by physical
  position.

## Reading order

1. `docs/README.md` — what lives where.
2. `docs/roadmap/README.md` — current focus.
3. `docs/decisions/README.md` — settled, open (with the phases they block) and deferred.
4. `plan/README.md` and the phase file of the task at hand.
5. `docs/design/` and `docs/command_reference/` pages the task touches.
