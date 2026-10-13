# Measurements on the reference GPU

GeForce GTX 1050 Ti 4 GB, on the owner's laptop (E-04). Each row records the build commit,
OS and driver, window size, and how the number was taken: GPU timer query, or wall clock.

| Date | Unit | Scene | Window | Preview scale | Frame ms (moving) | Frame ms (settled) | Shader compile ms | Commit, OS, driver |
|---|---|---|---|---|---|---|---|---|
| 2026-10-10 | P00.5 | gyroid_box.json | 1280×800 | 0.35 | 0.8 GPU / 3.8 wall (448×280) | 2.7 GPU / 5.3 wall | 27.7 | 53c6cb6, Windows 10, 3.3.0 NVIDIA 441.93 |
| 2026-10-10 | P00.5 | gyroid_box.json | 1280×800 | 0.5 | 1.1 GPU / 3.8 wall (640×400) | 2.6 GPU / 4.2 wall | 26.1 | 53c6cb6, Windows 10, 3.3.0 NVIDIA 441.93 |
| 2026-10-10 | P00.5 | gyroid_box.json | 1280×800 | 1 | 3.4 GPU / 4.8 wall (1280×800) | 2.6 GPU / 4.1 wall | 19.6 | 53c6cb6, Windows 10, 3.3.0 NVIDIA 441.93 |
| 2026-10-10 | P00.5 | tapered octet (P12) | 1280×800 | 0.35 | 80.6 GPU / 83.3 wall (448×280) | 385.3 GPU / 389.5 wall | 19.0 | 53c6cb6, Windows 10, 3.3.0 NVIDIA 441.93 |
| 2026-10-10 | P00.5 | tapered octet (P12) | 1280×800 | 0.5 | 127.6 GPU / 128.6 wall (640×400) | 375.7 GPU / 372.2 wall | 30.1 | 53c6cb6, Windows 10, 3.3.0 NVIDIA 441.93 |
| 2026-10-10 | P00.5 | tapered octet (P12) | 1280×800 | 1 | 402.0 GPU / 395.7 wall (1280×800) | 389.8 GPU / 360.9 wall | 26.7 | 53c6cb6, Windows 10, 3.3.0 NVIDIA 441.93 |
| 2026-10-10 | P00.5 | mesh_lattice.json | 1280×800 | 0.35 | 1.9 GPU / 3.9 wall (448×280) | 7.3 GPU / 8.2 wall | 22.1 | 53c6cb6, Windows 10, 3.3.0 NVIDIA 441.93 |
| 2026-10-10 | P00.5 | mesh_lattice.json | 1280×800 | 0.5 | 2.7 GPU / 3.8 wall (640×400) | 7.3 GPU / 8.3 wall | 23.4 | 53c6cb6, Windows 10, 3.3.0 NVIDIA 441.93 |
| 2026-10-10 | P00.5 | mesh_lattice.json | 1280×800 | 1 | 8.0 GPU / 8.9 wall (1280×800) | 7.3 GPU / 8.3 wall | 21.2 | 53c6cb6, Windows 10, 3.3.0 NVIDIA 441.93 |
| 2026-10-12 | P01.4 | 1 primitive | 1280×800 | 0.35 | 0.2 GPU / 4.1 wall (448×280) | 0.6 GPU / 3.3 wall | 156.8 | b011b24, Windows 10, 3.3.0 NVIDIA 441.93 |
| 2026-10-12 | P01.4 | 1 primitive | 1280×800 | 1 | 0.6 GPU / 3.8 wall (1280×800) | 0.6 GPU / 2.7 wall | 13.8 | b011b24, Windows 10, 3.3.0 NVIDIA 441.93 |
| 2026-10-12 | P01.4 | 3 primitives | 1280×800 | 0.35 | 0.3 GPU / 3.0 wall (448×280) | 1.1 GPU / 3.8 wall | 265.5 | b011b24, Windows 10, 3.3.0 NVIDIA 441.93 |
| 2026-10-12 | P01.4 | 3 primitives | 1280×800 | 1 | 1.0 GPU / 2.8 wall (1280×800) | 1.1 GPU / 2.6 wall | 17.6 | b011b24, Windows 10, 3.3.0 NVIDIA 441.93 |
| 2026-10-12 | P01.4 | 5 primitives | 1280×800 | 0.35 | 0.4 GPU / 3.5 wall (448×280) | 1.0 GPU / 3.0 wall | 412.9 | b011b24, Windows 10, 3.3.0 NVIDIA 441.93 |
| 2026-10-12 | P01.4 | 5 primitives | 1280×800 | 1 | 1.0 GPU / 2.8 wall (1280×800) | 0.9 GPU / 2.6 wall | 20.5 | b011b24, Windows 10, 3.3.0 NVIDIA 441.93 |
| 2026-10-12 | P01.4 | 10 primitives | 1280×800 | 0.35 | 1.1 GPU / 2.9 wall (448×280) | 3.6 GPU / 4.6 wall | 670.1 | b011b24, Windows 10, 3.3.0 NVIDIA 441.93 |
| 2026-10-12 | P01.4 | 10 primitives | 1280×800 | 1 | 3.9 GPU / 4.7 wall (1280×800) | 3.2 GPU / 4.1 wall | 22.2 | b011b24, Windows 10, 3.3.0 NVIDIA 441.93 |
| 2026-10-12 | P01.4 | 1 TPMS-in-box | 1280×800 | 0.35 | 0.8 GPU / 2.8 wall (448×280) | 3.1 GPU / 3.9 wall | 338.9 | b011b24, Windows 10, 3.3.0 NVIDIA 441.93 |
| 2026-10-12 | P01.4 | 1 TPMS-in-box | 1280×800 | 1 | 3.6 GPU / 4.3 wall (1280×800) | 2.6 GPU / 3.5 wall | 14.8 | b011b24, Windows 10, 3.3.0 NVIDIA 441.93 |
| 2026-10-12 | P01.4 | 3 TPMS-in-box | 1280×800 | 0.35 | 2.1 GPU / 3.0 wall (448×280) | 7.6 GPU / 8.5 wall | 1209.7 | b011b24, Windows 10, 3.3.0 NVIDIA 441.93 |
| 2026-10-12 | P01.4 | 3 TPMS-in-box | 1280×800 | 1 | 8.7 GPU / 9.4 wall (1280×800) | 7.6 GPU / 8.6 wall | 30.1 | b011b24, Windows 10, 3.3.0 NVIDIA 441.93 |

P01.4 rows: the conditions are under *Against object count (P01 T4)* below.

P00.5 conditions: `scripts/measure_viewport.py --field-view …` at commit 53c6cb6, Windows 10,
GeForce GTX 1050 Ti/PCIe/SSE2, driver 441.93 (GL 3.3.0), Qt 6.8.3 msvc2022_64, MSVC 14.44,
Release, display scaling 100%, mains power, Balanced power plan. *Window* is the traced
size. Each cell is the median of 60 frames: *GPU* is the `GL_TIME_ELAPSED` query, *wall* the
time between presented frames, vsync off. The moving cell gives the preview size traced.
Compile ms are with the driver's shader cache warm (second run). In the owner's first run,
one value each read 945.4 ms (mesh_lattice 0.35) and 327.1 ms (tapered octet 0.35), and all
the others 14–31 ms: the cold compiles. The raw JSON of both runs (9 + 9 lines, run 1
first) is in `02-measurements-p00.jsonl`. Three rows of the pasted report were garbled; they
were re-read from that file. On the heavy scene the median wall time can fall below the
median GPU time (tapered octet 1.0 settled: 360.9 against 389.8 ms). The two are medians of
different samples: up to 13 of the 60 GPU times are lost while every query is in flight.

## Against dualc_field_view (P00 T5)

Same laptop, same DualC pin, vsync off, a 1100×800 view (dualc_field_view's window).

The comparable numbers are the editor's `moving.wallMs` and dualc_field_view's frame time
during a held drag: dualc_field_view draws back to back only while the view moves (see the
checklist, step 5). Its frame time when still is not a render time, so it is not listed.

| Date | Scene | Preview scale | Editor moving / settled (wall ms) | dualc_field_view drag ms, median (mean, frames) | Editor against dualc_field_view, moving |
|---|---|---|---|---|---|
| 2026-10-10 | gyroid_box.json | 0.35 | 3.2 / 3.7 | 2.9 (2.9, 2020) | +10% (+0.3 ms) |
| 2026-10-10 | gyroid_box.json | 1.0 | 4.6 / 3.4 | 4.0 (4.1, 1468) | +15% (+0.6 ms) |
| 2026-10-10 | tapered octet (P12) | 0.35 | 77.7 / 333.2 | 73.0 (73.1, 76) | +6% |
| 2026-10-10 | tapered octet (P12) | 1.0 | 271.7 / 329.5 | 393.2 (376.0, 10) | −31%, not like for like: the viewer's window size was not checked |
| 2026-10-10 | mesh_lattice.json | 0.35 | 4.4 / 8.0 | 4.5 (4.5, 1314) | −2% |
| 2026-10-10 | mesh_lattice.json | 1.0 | 9.2 / 7.9 | 8.0 (7.9, 747) | +15% (+1.2 ms) |

dualc_field_view was timed with PresentMon 2.6.0, 6 s per capture. Same conditions as above.

Snapshots, editor against `dualc_field_view --snapshot`, 800×600, default view:

| Scene | Pixels that differ | Max channel diff |
|---|---|---|
| gyroid_box.json | 111 / 480 000 | 1 |
| tapered octet (P12) | 44 / 480 000 | 1 |
| mesh_lattice.json | 715 / 480 000 | 20 (the owner looked: no visible difference) |

## Findings (P00 T5)

- **The image matches dualc_field_view** on all three scenes (table above).
- **Frame time.** The moving frame time is within 10% of dualc_field_view on three of six
  rows. On the two light 1.0 rows (gyroid and mesh_lattice), the editor is 15% slower:
  +0.6 ms and +1.2 ms per frame. That may be a fixed cost per frame rather than a slower
  trace. The editor's wall time stays at or above about 3.8 ms at 1280×800 (3.2 ms at
  1100×800), even when the GPU trace takes 0.8 ms. That fits QOpenGLWidget composing its
  framebuffer into the window before each swap. mesh_lattice at 0.35 shows no such cost
  (−2%), so this is not settled. On the heavy scene, +6% at 0.35 is within the noise.
  The octet 1.0 row needs a re-run with the viewer's window checked at 1100×800. The
  viewer's 393 ms is close to the editor's 1280×800 times (390–402 ms). The scene is also
  noisy: the editor's own full-resolution frames at 1100×800 read 272 ms moving and 330 ms
  settled.
- **The tapered octet is not interactive on the reference GPU.** About 81 ms per moving
  frame at 0.35 and 128 ms at 0.5 (8–12 fps), and 375–390 ms per full frame. The owner
  saw the lag in the live window. dualc_field_view is no faster (73 ms at 0.35). The light
  scenes run at 3–9 ms, full resolution included.
- **No coloured squares on the GTX.** The blue-red-green squares on the silhouette while
  dragging (P00 T2) were seen on the Quadro FX 3700 (nouveau) only.
- **Open: the status bar's settled `frame` reads high on light scenes.** In the live window
  at 0.5 (docks closed, window size not fixed), `frame` / `preview` read 30 / 1.0 ms for the
  gyroid and 48 / 1.4 ms for mesh_lattice. The bench gives 2.6 / 1.1 and 7.3 / 2.7. The
  preview agrees, and the octet's frame and preview are both about 0.63× the bench's,
  which fits a smaller window. The owner's hypothesis, not yet verified: the single settled
  frame runs after the GPU has dropped its clocks at the end of the drag, while the bench's
  back-to-back frames keep it boosted. A test would time one full frame after an idle gap.

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
   laptop in to mains power, and close other GPU work. On Windows the build does not
   deploy Qt: run `windeployqt` on `DualCEditor.exe` (and on `dualc_field_view.exe` if it
   links Qt), or put Qt's `bin` on `PATH`. P00.5 used Qt 6.8.3 msvc2022_64 from
   aqtinstall, MSVC 14.44 and Ninja.
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
5. **Compare the frame time** with dualc_field_view. It prints no frame time. It draws
   back to back only while the view moves: when settled, it waits up to 0.1 s for input
   before each redraw (`examples/dualc_field_view.cpp`, the main loop). So only its frame
   time **during a held left-drag** is a render time. Compare it with the editor's
   `moving.wallMs`. Its frame time with the view still is the wait, not the trace. For each
   scene, at preview scales 0.35 and 1.0 (at 1.0 the drag frames are full resolution):
   - dualc_field_view, with its window at the default 1100×800. Do not resize or maximize
     it, and keep display scaling at 100%, or its framebuffer is not 1100×800:
     - Linux:
       `__GL_SYNC_TO_VBLANK=0 __GL_SHOW_GRAPHICS_OSD=1 __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia dualc_field_view --preview-scale S <graph>`.
       The NVIDIA overlay shows the fps (ms = 1000 / fps).
     - Windows: in the NVIDIA Control Panel, set *Vertical sync* to *Off* for
       `dualc_field_view.exe`. Capture the frame times with PresentMon, from an
       administrator shell, for a few seconds of continuous drag, and take the median.
       The Alt+R overlay needs GeForce Experience 3.20 or later.
   - The editor, at the same size:
     `DualCEditor --bench --bench-size 1100x800 --preview-scale S <graph>`, with the
     PRIME variables on Linux. Read `moving.wallMs`.

   Fill the second table. "Within about 10%" is the plan's bar.
6. **Look at the live window.** `DualCEditor --preview-scale 0.5`, then close the
   Parameters and Log docks:
   - Orbit each scene. Does it feel responsive at 0.35 and 0.5?
   - Do the *blue-red-green squares* the owner saw on the Quadro during a drag (P00 T2,
     on the silhouette while previewing) show on the GTX?
   - Do the status bar's `frame` and `preview` agree with the script's GPU numbers?

## Against object count (P01 T4)

The owner ran `scripts/measure_viewport.py --objects` on the GTX 1050 Ti on 2026-10-12.
Its twelve rows are in the first table above (unit `P01.4`). The raw JSON is
`02-measurements-p01.jsonl`. Conditions: commit b011b24 (the build matched; the pull to it
changed no C++), Windows 10 (10.0.19045), GeForce GTX 1050 Ti/PCIe/SSE2, driver 441.93
(GL 3.3.0), Qt 6.8.3, mains power, the editor closed during the run, no PRIME.

Scenes, written by `scripts/measure_viewport.py --objects`, one graph file per object, so
each is one object of a scene lowered to one shader (`../design/03-scene-lowering.md`):
- **1, 3, 5 and 10 primitives.** A fixed 5×2 grid of slots 1.2 apart in x and y,
  alternately a sphere of radius 0.4 and a box of side 0.8. No two overlap. The view frames
  the union, so the objects cover about the same share of the screen whatever their count.
- **1 and 3 TPMS-in-box.** `intersection(box(min=[-2,-2,-2],max=[2,2,2]),onion(normalize(gyroid(wavelength=1)),thickness=0.3))`
  (`gyroid_box.json` without its spherical hole), translated 5 apart along x. No two
  overlap.

Each at preview scales 0.35 and 1.0, traced at 1280×800, 60 frames per phase, nothing
selected. *Per added object* is the least-squares slope of the median ms against the object
count, over the counts of one family.

| Date | Family | Preview scale | Per added object, moving GPU / wall ms | Per added object, settled GPU / wall ms | Commit, OS, driver |
|---|---|---|---|---|---|
| 2026-10-12 | primitives (1, 3, 5, 10) | 0.35 | +0.10 / −0.10 | +0.34 / +0.12 | b011b24, Windows 10, 3.3.0 NVIDIA 441.93 |
| 2026-10-12 | primitives (1, 3, 5, 10) | 1.0 | +0.37 / +0.15 | +0.28 / +0.17 | b011b24, Windows 10, 3.3.0 NVIDIA 441.93 |
| 2026-10-12 | TPMS-in-box (1, 3) | 0.35 | +0.61 / +0.10 | +2.28 / +2.29 | b011b24, Windows 10, 3.3.0 NVIDIA 441.93 |
| 2026-10-12 | TPMS-in-box (1, 3) | 1.0 | +2.58 / +2.55 | +2.49 / +2.52 | b011b24, Windows 10, 3.3.0 NVIDIA 441.93 |

Findings (P01 T4):
- **The frame time grows with the object count.** At 1.0, settled GPU ms: primitives
  0.59 / 1.06 / 0.87 / 3.16 for 1 / 3 / 5 / 10 objects, about **+0.29 ms per added
  primitive**; TPMS-in-box 2.64 / 7.62 for 1 / 3, about **+2.5 ms per added box**. The
  5-primitive row is below the 3-primitive row: the owner reads it as noise, the frames
  being under 1.1 ms.
- **E-23 is triggered.** Its threshold, decided 2026-10-12 (`../decisions/03-deferred.md`),
  is any clear slope. Both families show one, and the TPMS slope is about one TPMS object's
  own cost per added object: without culling every step evaluates every object.
- **Headroom.** The worst scene, 3 TPMS-in-box, takes 7.6–8.7 GPU / 8.6–9.4 wall ms at 1.0,
  under 16 ms. Ten primitives take 3.2–3.9 GPU / 4.1–4.7 wall ms. The moving wall time at
  0.35 stays at 2.8–4.1 ms for every scene: the fixed per-frame cost P00 T5 saw, not the
  trace.
- **The cold compile grows with the object count.** Each scene's first run (at 0.35) is a
  cold compile: 157 / 265 / 413 / 670 ms for 1 / 3 / 5 / 10 primitives (about +57 ms per
  primitive), 339 / 1210 ms for 1 / 3 TPMS-in-box. The second run, from the driver's cache,
  takes 14–30 ms. This bears on E-21 (asynchronous compile).

Before the owner's run, on the development box only, to check the tooling, not a performance figure (E-04): llvmpipe
(Mesa 26.0.8, LLVM 21.1.8), 160×100, 4 frames, commit 2c9de42 plus the script change.
Settled GPU ms: primitives 1.8 / 4.3 / 4.0 / 15.3 for 1 / 3 / 5 / 10, TPMS-in-box 22.4 / 61.5
for 1 / 3. Per added object at 1.0, settled GPU: primitives +1.57 ms, TPMS-in-box +20.2 ms.

## With per-object culling (P07 T1)

The owner ran `scripts/measure_viewport.py --objects` on the GTX 1050 Ti on 2026-10-13, at
dd92129 (P07 T1, culling, `../design/03-scene-lowering.md` § Culling). Conditions as
P01 T4 above: Windows 10, driver 441.93 (GL 3.3.0), Qt 6.8.3, mains power, Release. The raw
JSON is `02-measurements-p07.jsonl`; the script labels its rows `P01.4`. The new family is
*N stacked in depth*: the same primitives in a column along the default view axis.

| Family | Preview scale | Per added object, settled GPU ms, b011b24 (no culling) | dd92129 (culling) |
|---|---|---|---|
| primitives (1, 3, 5, 10) | 1.0 | +0.28 | +0.40 |
| primitives (1, 3, 5, 10) | 0.35 | +0.34 | +0.40 |
| stacked in depth (1, 3, 5, 10) | 1.0 | — | +0.18 |
| TPMS-in-box (1, 3) | 1.0 | +2.49 | +1.90 |
| TPMS-in-box (1, 3) | 0.35 | +2.28 | +1.95 |

Settled GPU ms at 1.0: 10 primitives 3.2 → 4.2; 3 TPMS-in-box 7.6 → 6.6; 1 TPMS-in-box
2.6 → 2.8; 10 stacked in depth 2.3. Cold compile (first run, at 0.35): 10 primitives
670 → 2009 ms; 3 TPMS-in-box 1210 → 3503 ms. Warm compiles 17–93 ms. Wall times were
noisier than on 2026-10-12 in every scene (1 primitive settled wall 2.7 → 9.3 ms); read the
GPU columns.

`--snapshot` of all ten `--objects` scenes at 8b9baf8 and at dd92129: 0 / 480 000 px differ
in each. The owner opened 3 TPMS-in-box in the window: nothing missing, orbiting fine.

Findings (P07 T1):
- **The culling does not change the image** on the GTX (0 px in every scene), nor on llvmpipe
  (`scene_render_test`).
- **It does not lower the planar primitive slope** (+0.28 → +0.40 ms). For a sphere or a box,
  the box test costs about as much as the SDF it would skip. In addition, the first visible
  object in scene order is always evaluated, because nothing is nearer yet.
- **It lowers the TPMS slope by about a quarter** (+2.49 → +1.90 ms), not to near zero. The
  first object is paid on every ray, whichever box the ray is in.
- **The cold compile triples.** Each object's SDF now sits behind a non-uniform branch. This
  bears on E-21 (P07 T2).

These rows are at T1's first commit. On the owner's decision (2026-10-13, option C), the
culling now applies to costly objects only (`../design/03-scene-lowering.md` § The cost
rule). A scene of primitives is P01's shader again. P07 T3 re-measures at T2's commit.

### GTX 1050 Ti at 6f3a8bd (P07 T1, option C)

The owner ran `scripts/measure_viewport.py --objects --keep` on the GTX at 6f3a8bd (cull
only costly objects), same conditions as above. Its 20 raw lines are appended to
`02-measurements-p07.jsonl` (commit 6f3a8bd); the script still labels them `P01.4`.

| Family | Preview scale | Per added object, settled GPU ms, b011b24 (no culling) | dd92129 (culling all) | 6f3a8bd (option C) |
|---|---|---|---|---|
| primitives (1, 3, 5, 10) | 1.0 | +0.28 | +0.40 | **+0.29** |
| stacked in depth (1, 3, 5, 10) | 1.0 | — | +0.18 | +0.12 |
| TPMS-in-box (1, 3) | 1.0 | +2.49 | +1.90 | **+1.94** |
| TPMS-in-box (1, 3) | 0.35 | +2.28 | +1.95 | +1.98 |

Settled GPU ms at 1.0: 10 primitives 3.2 (dd92129 4.2); 3 TPMS-in-box 6.6 (dd92129 6.6).
`--snapshot` of all ten `--objects` scenes at 8b9baf8 and at 6f3a8bd: **0 / 480 000 px**
differ in each.

Compile times in this run were 12–60 ms for every scene, TPMS included (25–28 ms for 3
TPMS-in-box at 0.35). The driver's disk shader cache most likely served them. **So this run
does not measure cold compile**, and the claim "primitive compile back near 670 ms cold" is
not checked. The cold figure is P07 T3's (`plan/35_P07_render_scaling.md`), after the GL
driver's shader cache is cleared.

### GTX 1050 Ti at b163141 (P07 T3, cold cache)

On 2026-10-13, the owner ran the P07 T3 checklist below on the GTX at b163141. The build
matched; it compiled the same C++ as 4b89008 (P07 T2's asynchronous compile, option C's
culling). Conditions as above: Windows 10, driver 441.93 (GL 3.3.0), Qt 6.8.3, mains
power, Release, no other editor window. `%LOCALAPPDATA%\NVIDIA\GLCache` (138 files,
30.7 MB) was emptied right before the script. The 20 raw lines, labelled `P07.3`, are
the last 20 of `02-measurements-p07.jsonl`.

| Family | Preview scale | Per added object, settled GPU ms, b011b24 (no culling) | dd92129 (culling all) | 6f3a8bd (option C) | b163141 (option C, T2) |
|---|---|---|---|---|---|
| primitives (1, 3, 5, 10) | 1.0 | +0.28 | +0.40 | +0.29 | **+0.29** |
| primitives (1, 3, 5, 10) | 0.35 | +0.34 | +0.40 | — | +0.34 |
| stacked in depth (1, 3, 5, 10) | 1.0 | — | +0.18 | +0.12 | **+0.13** |
| stacked in depth (1, 3, 5, 10) | 0.35 | — | — | — | +0.11 |
| TPMS-in-box (1, 3) | 1.0 | +2.49 | +1.90 | +1.94 | **+1.96** |
| TPMS-in-box (1, 3) | 0.35 | +2.28 | +1.95 | +1.98 | +1.95 |

Settled GPU ms at 1.0: 1 / 3 / 5 / 10 primitives 0.6 / 1.1 / 0.8 / 3.2; 1 / 10 stacked
in depth 0.6 / 1.9; 1 / 3 TPMS-in-box 2.7 / 6.6. Wall times were noisy (10 primitives
settled at 1.0: 3.2 GPU, 17.6 wall ms); read the GPU columns.

Cold compile, each scene's first run (at 0.35), GUI thread (`--bench` uses the
synchronous `setScene`):

| Scene | b011b24 (P01.4) | b163141 (P07.3) |
|---|---|---|
| 1 primitive | 157 | 152 |
| 3 primitives | 265 | 399 |
| 5 primitives | 413 | 512 |
| 10 primitives | 670 | **1137** |
| 1 TPMS-in-box | 339 | 757 |
| 3 TPMS-in-box | 1210 | **1897** |

The stacked-in-depth compiles (4–36 ms) are not cold. Their shaders likely match the
side-by-side primitive scenes' except for values, so the cache served them; this was not
checked. The runs at 1.0 hit the cache of the run at 0.35 (6–25 ms).

**Live window, cold** (the cache emptied again). `DualCEditor tpms_box_0.fld tpms_box_1.fld`
was opened in the script's kept folder, and `tpms_box_2.fld` was dropped on it during a held
orbit. The view kept turning while the status bar read `compile …`, and the owner's
verdict was "all good". Then the status bar read `shader compile 2566.6 ms`, timed on the
compile thread's shared context (compile and link only, as `--bench`'s `compileMs`).

Findings (P07 T3):
- **The planar primitive slope is P01.4's** (+0.29 against +0.28 ms): under option C,
  primitives are not culled.
- **The TPMS-in-box slope is down about a fifth** (+2.49 → +1.96 ms), the same as at
  dd92129 and 6f3a8bd.
- **The depth-stacked column is cheap**, at +0.13 ms per object.
- **The window stays responsive** during a cold 2.6 s compile.
- **Not explained: the cold compiles are longer than P01.4's.**
  - Primitives: 10 primitives take 1137 ms against 670, although option C gives
    primitives P01's shader.
  - TPMS-in-box: 1 and 3 take 757 / 1897 ms against 339 / 1210. A culled object's SDF
    sits behind a branch, which can explain part of this.
  - The window's cold 3-TPMS compile, 2567 ms on the compile thread, is longer than the
    bench's 1897 ms on the GUI thread. A slower link on a second context would fit
    (`HANDOFF_P07_TASK2.md`). Both figures time the same span:
    `TraceRenderer::linkProgram` starts its clock after the codegen and the shader
    assembly, and stops it at the link status. So the gap is not the codegen. There is one
    sample of each.
  - This is open (`HANDOFF_P07_TASK3.md`), not a pass.

### How to measure (P07 T3 checklist)

P07 T3 re-measures at T2's commit or later (4b89008 and after; the P07.3 commit changes
only the script and the docs). Same set-up as the P01 T4 checklist below: a Release build,
mains power, other GPU work closed, the renderer checked.

1. **Clear the driver's shader cache**, with every DualCEditor window closed, so that each
   scene's first run (at 0.35) is a cold compile. P07 T1's run at 6f3a8bd skipped this, and
   its compiles (12–60 ms) came from the cache.
   - Windows: delete the contents of `%LOCALAPPDATA%\NVIDIA\GLCache`.
   - Linux: delete `~/.nv/GLCache` (or `$__GL_SHADER_DISK_CACHE_PATH` if set).
2. **Time the scenes.** From the repository root:
   - Windows: `python scripts/measure_viewport.py --objects --unit P07.3 --keep`
   - Linux PRIME: add `--prime`.

   Twenty runs: ten scenes (1, 3, 5, 10 primitives side by side; the same stacked in depth;
   1, 3 TPMS-in-box) at 0.35 and 1.0. The script prints the `GPU: …` line, twenty rows, six
   *per added object* lines and the folder it kept. It appends the raw JSON to
   `measurements-<date>.jsonl`.
3. **The live window, cold.** Clear the shader cache again (step 1), so the window's
   compile is not served by step 2. In the kept folder, open the first two TPMS-in-box:
   `DualCEditor tpms_box_0.fld tpms_box_1.fld`. Wait for the image, then start orbiting
   with a held drag and, while still orbiting, drop `tpms_box_2.fld` on the window (or
   `File ▸ Open` it). Note:
   - whether the view kept turning while the status bar read `compile …`;
   - the `compile C ms` the status bar shows once the three boxes are on screen.
4. **Paste back** the `GPU: …` line, the twenty rows, the six growth lines, the raw JSON
   lines, the two answers of step 3, and whether the build was the one at the printed
   commit.

### How to measure (P01 T4 checklist)

Same set-up as the P00 T5 checklist above, steps 1 and 2: a Release build at the commit to
measure, mains power, other GPU work closed, the renderer checked.

1. From the repository root:
   - Linux PRIME: `python scripts/measure_viewport.py --objects --prime`
   - Windows: `python scripts/measure_viewport.py --objects`
     (add `--exe build\bin\DualCEditor.exe` if the binary is elsewhere)

   Twelve runs: six scenes at 0.35 and 1.0. The script prints the `GPU: …` line, twelve
   rows for the first table and four *per added object* lines. It appends the raw JSON to
   `measurements-<date>.jsonl` in the working directory.
2. Paste back the `GPU: …` line, the twelve rows, the four growth lines and the raw JSON
   lines, and say whether the build was the one at the printed commit.
3. Optional, in the live window: add `--keep`, and the script prints the folder that holds
   the scene files. Open the ten primitives, `DualCEditor primitive_*.fld` in that folder,
   and orbit at `--preview-scale 0.35` and `1.0`. Do ten objects feel as responsive as one?

## Imported meshes (P04 T2)

E-22's trigger (`03-go-no-go.md`, item 3): frame time with several mesh objects. Owed on
the GTX 1050 Ti; the dev box cannot measure (E-04).

```bash
python scripts/measure_viewport.py --meshes --prime
```

Scenes of 1, 3 and 5 closed UV spheres (radius 0.4, 24 × 48, written as OBJ), side by
side at P01's primitive positions, each imported as a `mesh` node baked to 96³, at preview
scales 0.35 and 1.0, 1280 × 800. Paste the rows and the per-object growth line here.
E-22 fires when the settled frame at 1.0 misses the frame target of `03-go-no-go.md`.

| Date | Unit | Scene | Scale | compile ms | moving GPU ms | settled GPU ms | Where |
|---|---|---|---|---|---|---|---|
| — | P04.2 | owed | | | | | GTX 1050 Ti |
