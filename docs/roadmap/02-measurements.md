# Measurements on the reference GPU

GeForce GTX 1050 Ti 4 GB, on the owner's laptop (E-04). Each row records the build commit,
OS and driver, window size, and how the number was taken: GPU timer query, or wall clock.

| Date | Unit | Scene | Window | Preview scale | Frame ms (moving) | Frame ms (settled) | Shader compile ms | Commit, OS, driver |
|---|---|---|---|---|---|---|---|---|

## Against dualc_field_view (P00 T5)

Same laptop, same DualC pin, vsync off, a 1100×800 view (dualc_field_view's window).

| Date | Scene | Preview scale | Editor ms (moving / settled, wall) | dualc_field_view ms (moving / settled) | Snapshot diff (800×600) |
|---|---|---|---|---|---|

## How to measure (P00 T5 checklist)

The editor's numbers come from `DualCEditor --bench` (docs/command_reference/01-viewport.md
§ Measurement run), driven by `scripts/measure_viewport.py`. Each run:
- shows a bare viewport whose traced size is 1280×800 device pixels;
- orbits it one step per presented frame for 60 frames (*moving*: the preview at the
  given scale, as under a held drag), then draws 60 full-resolution frames of a still
  camera (*settled*);
- reports, per phase, the median `GL_TIME_ELAPSED` time of a frame (*GPU*) and the median
  time between presented frames (*wall*, which adds Qt's composition and the swap);
- also reports the GL compile + link ms and the ms from `show()` to the first frame.

Vsync is off for the run (swap interval 0), so the wall time is not rounded to the
display's refresh.

1. **Build** a Release tree at the commit to measure (`CLAUDE.md` § Build). Plug the
   laptop in to mains power, and close other GPU work.
2. **Check the GPU.** On Linux PRIME:
   `__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia glxinfo -B | grep -i renderer`
   must name the GTX 1050 Ti. On Windows, `gpu_preference.h` asks the driver for the
   discrete GPU. The script prints the renderer it got: if it is not the GTX, stop.
3. **Time the three scenes.** From the repository root:
   - Linux PRIME: `python scripts/measure_viewport.py --prime`
   - Windows: `python scripts/measure_viewport.py`
     (add `--exe build\bin\DualCEditor.exe` if the binary is elsewhere)

   The scenes are DualC's `gyroid_box.json`, recipe P12 of DualC's
   `docs/command_reference/12-dualc_field_view/` (a tapered octet), and `mesh_lattice.json`,
   each at preview scales 0.35, 0.5 and 1.0: nine runs. The script writes `cube.obj`
   beside a copy of `mesh_lattice.json`, since DualC only generates it at build time. It
   prints nine rows for the first table above and appends the raw JSON to
   `measurements-<date>.jsonl` in the working directory. Paste the rows and the
   `GPU: …` line back. The driver caches compiled shaders on disk, so only a scene's
   first run ever gives a cold compile ms: on llvmpipe the tapered octet took 7.2 s cold
   and 0.95 s cached. Say whether the scenes had been opened on the laptop before.
4. **Compare the image** with dualc_field_view at the same DualC pin. Build DualC's
   viewer (`cmake -S build/_deps/dualc-src -B build-dualc -DCMAKE_BUILD_TYPE=Release
   -DDUALC_BUILD_FIELD_VIEW=ON`, then `cmake --build build-dualc --target dualc_field_view`),
   then add `--field-view <path to dualc_field_view>` to step 3.
   The script renders each scene through both `--snapshot`s at 800×600 and prints the
   pixels that differ. On the development box, the gyroid differed in 198 of 480 000
   pixels, by at most 2 per channel (P00 T1).
5. **Compare the frame time** with dualc_field_view. It prints no frame time, but it draws
   continuously, so with vsync off its frame rate gives its wall ms per frame (1000 / fps).
   For each scene, at preview scales 0.35 and 1.0:
   - dualc_field_view, window left at its default 1100×800 and not resized:
     - Linux:
       `__GL_SYNC_TO_VBLANK=0 __GL_SHOW_GRAPHICS_OSD=1 __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia dualc_field_view --preview-scale S <graph>`.
       The NVIDIA overlay shows the fps.
     - Windows: in the NVIDIA Control Panel, set *Vertical sync* to *Off* for
       `dualc_field_view.exe`, and read the fps from the NVIDIA overlay (Alt+R) or
       PresentMon.

     Read the fps with the view still (settled), then while holding a left-drag (moving).
   - The editor, at the same size:
     `DualCEditor --bench --bench-size 1100x800 --preview-scale S <graph>`, with the
     PRIME variables on Linux. Read `moving.wallMs` and `settled.wallMs`.

   Fill the second table. "Within about 10%" is the plan's bar. Display scaling must be
   100%, or dualc_field_view's framebuffer is not 1100×800.
6. **Look at the live window.** `DualCEditor --preview-scale 0.5`, then close the
   Parameters and Log docks:
   - Orbit each scene. Does it feel responsive at 0.35 and 0.5?
   - Do the *blue-red-green squares* the owner saw on the Quadro during a drag (P00 T2,
     on the silhouette while previewing) show on the GTX?
   - Do the status bar's `frame` and `preview` agree with the script's GPU numbers?
