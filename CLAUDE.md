# DualCEditor — agent entry

DualCEditor is a standalone Qt-based, fully raytraced 3D editor for DualC field graphs: import meshes,
build scenes with geometric operations (booleans, lattices, transforms), and extract high-quality meshes.
Sphere-traced SDF rendering with OpenGL 3.3 core, Qt 5/6 UI, multi-object viewport.

Built on [[DualC]] (engine library, pinned via FetchContent) and Qt (find_package).

## Build, test, gate

A bare clone configures with no flags; Qt 5 or Qt 6 is required (`find_package(Qt5 ...)`).
DualC is fetched at its pinned commit in the build tree, unless `-DDUALCEDITOR_DUALC_DIR=/path` 
names a local tree. GLFW and glad are fetched/vendored as DualC does.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DDUALCEDITOR_BUILD_TESTS=ON
cmake --build build --config Release -j
ctest --test-dir build -C Release --output-on-failure
```

Opt-in, all `OFF` by default: `-DDUALCEDITOR_WERROR=ON` (own-target warnings are errors).
The main app lands in `build/bin/DualCEditor` (or `Release/` with VS).

The gate is `scripts/check.py` — docs and hygiene only at first, no CTest. Run before every commit:

```bash
python scripts/check.py          # docs contract + repo hygiene
python scripts/check.py --fast   # docs + hygiene only, about a second
git config core.hooksPath scripts/hooks   # once per clone
```

## Five principles (inherited from DualC)

1. **Nothing stays in the chat.** Every fact, fix, decision goes into the repo before the session
   closes. Route by owner: `docs/README.md`.

2. **Every level has a README.** Each folder's `README.md` indexes files below it.

3. **Numbers are IDs.** Roadmap blocks, tracked items, reference pages keep their birth number.
   No renumbering; no reuse.

4. **Docs grow with the code.** A CLI surface or UI feature created/changed/removed gets its
   `docs/command_reference/` page updated in the same session.

5. **No page outgrows the cap.** Oversized pages split into same-numbered folders, never frozen.

## Reading order

1. `docs/README.md` — ownership of fact categories.
2. `docs/design/README.md` — how the editor fits: scene model, rendering pipeline.
3. `docs/decisions/README.md` — deferred, dropped, and settled choices.
4. `docs/roadmap/README.md` — current status and next-up items.
5. The roadmap block of the topic you touch.

## Key constraints

- **Library scope (D-04, inherited):** Scene management, UI, mesh import, and 3D transform
  stay in the editor. DualC is engine + ABI only. Meshes arrive in one global frame.
  
- **Dependency policy (D-01, inherited):** Permissive licenses only (MIT, BSD, PD).
  Qt is LGPL; dynamic linking of this app does not inherit the constraint to the library.
  If in doubt, ask.
  
- **Main branch is protected (inherited from DualC 2026-10-07):** Branch → PR → merge only.
  No direct pushes to main.

- **Target GPU:** GeForce RTX 1050 Ti 4GB. Baseline: interactive at preview scale, settle
  to full when idle.

## Links

- **Parent:** [[https://github.com/Giacogiak/DualC]] (engine library, pinned at a commit in CMakeLists.txt).
- **Sister client:** [[project_boletus_client_inmem_meshes]] — .NET Rhino/GH plugin (pinned dependency pattern).
- **Design plan:** `/home/giak/.claude/plans/this-is-a-divergence-adaptive-orbit.md`.
