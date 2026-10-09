# DualCEditor

A standalone, fully raytraced 3D editor for DualC field graphs. Import meshes, build scenes with geometric operations (booleans, lattices, transforms), and export high-quality meshes using sphere-traced SDF rendering.

## Features (planned)

- **Multi-object viewport:** Drag-and-drop mesh import, scene graph with show/hide
- **Operations:** Hard/smooth booleans, auto-watertight import, TPMS fill, strut-lattice fill
- **Real-time preview:** Sphere-traced rendering at adaptive resolution (full-res on idle)
- **Scene format:** `.dcscene` container (graph JSON + embedded meshes + optional baked grids)
- **Export:** STL, 3MF, OBJ with tiling support for large lattices

## Building

**Requirements:**
- CMake 3.20+
- C++17 compiler
- Qt 5.15+ or Qt 6
- OpenGL 3.3 core capable GPU

```bash
# Clone and configure
git clone https://github.com/Giacogiak/DualCEditor.git
cd DualCEditor
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build and run
cmake --build build --config Release -j
./build/bin/DualCEditor
```

To use a local DualC checkout instead of fetching:
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DDUALCEDITOR_DUALC_DIR=/path/to/DualC
```

## Development

Gate check (docs + hygiene):
```bash
python scripts/check.py
```

See [CLAUDE.md](./CLAUDE.md) for design principles and workflow.

## Roadmap

- **Phase 0:** Spikes (QOpenGLWidget host, id-returning codegen)
- **Phase 1–2:** Core architecture (scene model, async compile)
- **Phase 3–5:** Full editor shell and operations

See [docs/roadmap/](./docs/roadmap/) for current status.

## License

MIT (this editor). Links to DualC (MIT) and depends on Qt (LGPL, dynamic link only).

## Related

- **DualC:** Geometry library this editor is built on ([github.com/Giacogiak/DualC](https://github.com/Giacogiak/DualC))
- **Boletus:** .NET Rhino/Grasshopper client for DualC ([github.com/Giacogiak/Boletus](https://github.com/Giacogiak/Boletus))
