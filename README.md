# DualCEditor

A standalone 3D editor for [DualC](https://github.com/Giacogiak/DualC) field graphs. It is
in early development: the skeleton builds against DualC, and the viewport is being built
(see `docs/roadmap/README.md`).

What it will do:
- render solids, TPMS and strut lattices live by sphere tracing, without meshing them;
- import meshes by drag and drop, including mesh soups made watertight;
- combine objects with hard and smooth booleans, and fill volumes with TPMS or strut
  lattices;
- show, hide and select objects from a scene panel;
- save scenes as `.dcscene`, and export meshes to STL, 3MF or OBJ through DualC.

## Build

You need CMake 3.28 or newer, a C++17 compiler, Qt 6.4 or newer, and a GPU with OpenGL 3.3
core.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/bin/DualCEditor
```

To build against a local DualC checkout, add `-DDUALCEDITOR_DUALC_DIR=/path/to/DualC`.

## License

MIT. Qt is used under the LGPL and linked dynamically.
