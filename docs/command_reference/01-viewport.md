# 01 — Viewport, files and launch flags

`DualCEditor [options] [graph...]`

`graph` is a field graph: a `.json` document, or a `.fld` file in DualC's text shorthand
(`difference(box(min=[-1,-1,-1],max=[1,1,1]),sphere(radius=1.25))`). A name ending in
`.json` is read as JSON. Any other name is sniffed: a leading `{` is JSON, anything else is
shorthand, as in `dualc_field_view`. With no `graph`, the window opens on DualC's
`examples/samples/gyroid_box.json`, which is embedded in the binary from the pinned DualC
tree. A graph opens on the framed default view of `dualc_field_view`:
yaw 0.7, pitch 0.5, distance 1.3 × the diagonal of the trace box. The trace box is the
field's own bounds padded by 5%.

## Flags

| Flag | Default | Effect |
|---|---|---|
| `graph` (positional) | `gyroid_box.json` | The graphs to open, or meshes to import (`.obj`, `.ply`, `.stl`: one object each, as a drop imports them, *Importing a mesh* below). Each graph becomes one object of a scene, in the order given, lowered to one shader (*Several graphs* below). In the window the first is opened and the others added (*Objects* below). A `.dcscene` opens that scene (*Scene files* below); `--snapshot` and `--bench` take it alone, with no graphs after it. In the window, a graph that does not load still opens the window, with the error in the status bar and the log, and is watched, so fixing it renders it. |
| `--snapshot PATH` | off | Render one 800×600 frame of `graph` (default view) off-screen to the PNG `PATH`, then exit. Opens no window. |
| `--preview-scale S` | `0.5` | Resolution factor while the view moves. The view is traced at `S` × the window's pixels, then upscaled linearly. It returns to full resolution 0.18 s after the last input. Clamped to 0.1 – 1. `1` turns the preview off. A value that is not a number exits 1. Same flag and default as `dualc_field_view`. |
| `--bench` | off | Time `graph` while orbiting and when settled, print one JSON line on stdout, and exit. See *Measurement run*. Vsync is off for the run. |
| `--bench-size WxH` | `1280x800` | Traced size of `--bench`, in device pixels. |
| `--bench-frames N` | `60` | Frames timed in each `--bench` phase, after 3 warm-up frames. |
| `-h`, `--help` | | Print usage and exit. |
| `-v`, `--version` | | Print the version and exit. |

Exit codes of `--snapshot`:

| Code | Meaning |
|---|---|
| 0 | PNG written. |
| 1 | Shader link or PNG write failed (the reason goes to stderr). |
| 2 | The graph cannot be read (`cannot read <path>: …`), or does not parse or compile (`field-graph error at <pointer>: …`). |
| 77 | No OpenGL 3.3 core context. The ctest reports this as skipped. |

On Linux with no `DISPLAY` and no `WAYLAND_DISPLAY`, `--snapshot` selects the `offscreen`
Qt platform unless `QT_QPA_PLATFORM` is set. With no X display that platform has no GL,
so the result is exit 77.

## Recipes

```bash
./build/bin/DualCEditor                                   # open the viewport
./build/bin/DualCEditor part.fld                          # open (and watch) a graph
./build/bin/DualCEditor a.fld b.fld c.fld                 # a window on a scene of three objects
./build/bin/DualCEditor --preview-scale 0.25              # coarser, faster preview while moving
./build/bin/DualCEditor --snapshot out.png                # one frame, hardware GL
./build/bin/DualCEditor --snapshot out.png part.json      # one frame of part.json
./build/bin/DualCEditor --snapshot out.png a.fld b.fld c.fld   # a scene of three objects
LIBGL_ALWAYS_SOFTWARE=1 ./build/bin/DualCEditor --snapshot out.png   # Mesa llvmpipe
xvfb-run -s "-screen 0 1024x768x24" ./build/bin/DualCEditor --snapshot out.png  # no display (not yet run: xvfb is not installed on the dev box)
```

## Several graphs

With more than one `graph`, the window, `--snapshot` and `--bench` build a scene of one object per
graph and draw it through one shader (`../design/03-scene-lowering.md`). Each object keeps
its graph's own position: the transform is the identity, so graphs that overlap in space
overlap on screen (place them with `translate(...)` in the graph). Colours follow the order
of the arguments: the first is DualC's grey, then orange, blue, green, yellow, purple,
teal, pink, and round again. Every object is visible. The view is framed on the union of
the objects' trace boxes. A graph that does not read, parse or compile exits 2, and the
message names the object (`object 2 (b.fld): …`). In the `--bench` JSON, `graph` is the
paths joined by spaces.

```bash
printf 'translate(by=[-2,0,0],sphere(radius=0.8))' > a.fld
printf 'box(min=[-0.6,-0.6,-0.6],max=[0.6,0.6,0.6])' > b.fld
printf 'translate(by=[2,0,0],sphere(radius=0.8))' > c.fld
./build/bin/DualCEditor --snapshot three.png a.fld b.fld c.fld   # grey, orange, blue
```

## Opening a graph

| Way | Behaviour |
|---|---|
| `File ▸ Open…` (`Ctrl+O`, the platform's Open shortcut) | File dialog filtered on `*.dcscene *.json *.fld`, then on scenes only, graphs only, and `All files`. Starts in the last file's folder. A graph **replaces** the scene with one object; a `.dcscene` replaces it with the saved scene (*Scene files* below). |
| `File ▸ Add…` (no shortcut) | The same dialog, several files allowed. **Adds** one object per file. |
| The `graph` arguments | The first is opened, the others added. |
| Drop files on the window | Accepted when the drag carries a local file ending in `.json`, `.fld`, `.dcscene`, `.obj`, `.ply` or `.stl` (any case). A mesh file is imported (*Importing a mesh* below). Other files are refused: the cursor shows no drop. A `.dcscene` (the first, if several) **opens**, replacing the scene. Then the drop **adds** one object per graph file the drag carries, in order, to the scene on screen (the default `gyroid_box.json` included: delete it from the Objects dock). A drop anywhere on the window counts, the viewport, the docks and the log included. |

Opening or adding frames the camera on the scene's trace box (the union of every object's
box), like `F`. The window title is `<object name> - DualCEditor` with one object,
`N objects - DualCEditor` with several. An object is named after its file.

A relative mesh path (`mesh(path="cube.obj")`) is looked up beside the graph file first,
then in the working directory. `dualc_field_view` looks only in the working directory.

### Importing a mesh

A dropped `.obj`, `.ply` or `.stl` (or one named by `File ▸ Add…` or on the command
line) becomes one object named after the file. The file is not watched.

| The mesh is | The object is | Objects dock |
|---|---|---|
| Watertight (no open, no non-manifold edge, DualC's counts) and oriented outward | a `mesh` node: DualC's signed distance to it | `name  [mesh]` |
| Anything else: open, a soup, non-manifold, inside out | a `winding` node: inside is where the generalized winding number is over ½ | `name  [winding]` |

The log gives the counts read and welded and what was found (`watertight`, or `4 open
edges`, `normals inward`, …). A file that does not read gives its message in the status bar
and the log (`../design/07-mesh-import.md`).

`Edit ▸ Make Watertight` (enabled on a selected `[winding]` object) contours its winding
field into a closed mesh on a worker thread and replaces the object with a `[mesh]` one,
one undo step (`Make watertight <name>`). A dialog shows the stage (sampling, then
contouring) and its progress; the window keeps drawing. **Cancel** stops the sampling or
the contouring and leaves the object as it was. The preview bake that follows the result
**cannot be cancelled** (DualC D-45); the dialog says so.

A `.dcscene` carries imported meshes as `meshes/<id>.obj` entries.

### Scene files

A scene is saved to a `.dcscene` file (`../design/05-dcscene-format.md`). It holds every
object: its graph, name, transform, visible flag, colour, parameter edits and operands, and
the bytes of every mesh its graph reads. It names no absolute path, so it opens from any
folder, on any machine.

| Way | Behaviour |
|---|---|
| `File ▸ Save Scene` (`Ctrl+S`, the platform's Save shortcut) | Writes the scene to the `.dcscene` last opened or saved. With none yet, asks for a name, as `Save Scene As…`. |
| `File ▸ Save Scene As…` (`Ctrl+Shift+S`, the platform's Save As shortcut) | File dialog filtered on `*.dcscene`. Starts at the last scene file, or beside the last graph with its name and `.dcscene`. A name with no `.dcscene` gets it. |
| `File ▸ Open…`, a drop, the first argument | Opens a `.dcscene`: it **replaces** the scene and frames it. The undo history is cleared. |

The status bar says `Saved <name>: N objects`; the log names the file. A save that fails
says `Not saved: <reason>` (`cannot read mesh cube.obj of object 2 (mesh.fld): …`), and the
file on disk is left as it was.

A file that is not a scene, has no version or has a newer one is refused. The status bar
and the log say why, and the scene stays:
`future.dcscene is scene version 2, written by a newer DualCEditor; this DualCEditor reads
version 1`. `File ▸ Add…` refuses a `.dcscene`: a scene is opened, not added.

An object's graph file, named relative to the scene file, is watched again when it is still
there. Only a later change to it reloads it: until then the object shows the graph saved in
the scene, with its parameter edits. The scene's meshes are extracted to a temporary folder
that is removed when the editor exits.

### Reload on save

Every object's file is watched (`QFileSystemWatcher`, the file and its folder). When one
changes on disk, it is re-read 0.1 s after the last change signal, and re-rendered only if its
text differs from the text last loaded. Its new graph replaces the graph of every object made
from it; the other objects keep their parameter edits, visibility and colour. Files that
change together reload as one. The camera is kept, as in `dualc_field_view`. Press `F`
to frame the new bounds. A save that renames a new file over the old one (as many editors
do) is followed too. A save with the same text, or a change to another file in the folder,
loads and paints nothing. A mesh file the graph names is not watched: editing it does not
reload.

### While a scene compiles

Opening, adding, deleting and reloading compile the scene's shader off the GUI thread. While
it compiles:

- the viewport keeps showing the previous scene, and orbit, pan, zoom, `F`, picking and the
  docks keep working on it;
- the status bar's right side reads `compile …`; the left side keeps the last message until
  the new scene is on screen (`Loaded …`) or fails;
- a hide, show, recolour or parameter edit made meanwhile is kept by the new scene, for the
  objects it keeps;
- another open, add, delete or reload replaces the edit in flight: the window shows the
  last edit's scene, with every object added meanwhile. The replaced edit is never shown.

The new scene appears in one step, at its first frame. A scene that fails to compile leaves
the previous one on screen, as below. If an add fails while another add replaced it, both
files are watched, and each one is added by its next save.

### Errors

A graph that cannot be read, a field-graph error, or a shader compile or link error goes to:

- the **status bar**, as one line: `<file>: field-graph error at <JSON pointer>: <message>`,
  plus `(keeping the previous scene)` when a scene is on screen;
- the **Log** dock, with a time stamp and the full message (a shader log is several lines).

The previous scene, and its image, stay on screen. Nothing repaints. The failing file is
watched, so the next save that fixes it opens it (`File ▸ Open…`, first argument) or adds it
(a drop, `File ▸ Add…`, further arguments). A lowering error names the object:
`field-graph error at /root/in/1: object 2 (b.fld): unknown op 'no-such-op'`. For the shorthand,
the pointer is a character offset.

## Window

| Part | Content |
|---|---|
| `File` menu | `Open…` (`Ctrl+O`), `Add…`, `Save Scene` (`QKeySequence::Save`, `Ctrl+S`), `Save Scene As…` (`QKeySequence::SaveAs`, `Ctrl+Shift+S`), `Export Selected Object…` (`Ctrl+E`, greyed out with nothing selected), `Export Visible Scene…` (`Ctrl+Shift+E`), both greyed out while an export runs, `Quit` (`Ctrl+Q`, the platform's Quit shortcut). See *Scene files* and *Export*. |
| `Edit` menu | `Undo <step>` (`QKeySequence::Undo`, `Ctrl+Z`) and `Redo <step>` (`QKeySequence::Redo`: `Ctrl+Shift+Z` on Linux, `Ctrl+Y` on Windows), named after the step they undo or redo; greyed out with nothing to undo or redo. `Rename Object` (`F2`) and `Delete Object`: act on the selected object, greyed out with nothing selected. See *Undo and redo* below. |
| `Operations` menu | `Union`, `Intersection`, `Difference`, `Xor`, `Smooth Union`, `Smooth Intersection`, `Smooth Difference` (one per DualC boolean, see *Operation list*), and the `Blend k` spin box of the smooth ones, greyed out with fewer than two objects selected (see *Booleans* below); `Fill with TPMS…` and `Fill with Lattice…`, greyed out with no object selected (see *Fills* below). No shortcut. |
| `View` menu | `Objects`, `Transform`, `Parameters` and `Log`: show or hide each dock. `Section`: the same `X`, `Y`, `Z`, `−` and `+` as the toolbar. |
| Section toolbar | Under the menu: `Section [X] [Y] [Z] [−] [+] x = 1.20 mm`. A cut through the scene along one axis, moved in steps. See *Section* below. |
| Status bar, left | The last load: `Loaded <file>: K objects, N nodes, M parameters` (the whole scene's counts, the per-object `transform` node and `visible`/`colour` uniforms included), `Removed <name>`, or the last error. |
| Objects dock | On the right, above Transform. One row per object. See *Objects* below. |
| Transform dock | On the right, between Objects and Parameters. The selected object's position, rotation and scale, typed in millimetres and degrees. See *Transform* below. |
| Status bar, right | Always shown: `compile C ms   frame F ms`, then `   preview P ms` once a preview frame has been timed. `-` means not measured yet. While a scene compiles it reads `compile …`. See *Timing* and *While a scene compiles* below. |
| Parameters dock | On the right. The parameters of the selected object, or of every object when none is selected. See *Parameters* below. |
| Log dock | At the bottom, five lines high, read-only. One line per load (with its shader compile ms), reload and error. |

Each dock can be closed, floated or moved to another side. With both docks open, the default
1200×800 window leaves the viewport about 956×661.

Menu shortcuts are Qt's standard key sequences, matched on the character typed, so they
follow the keyboard layout (Belgian AZERTY included).

## Objects

The Objects dock lists the scene's objects in scene order (the order they were opened or
added). Each row is a visibility checkbox, a colour swatch and the object's name (its file
name; the tooltip gives the object id).

| Input | Effect |
|---|---|
| Click the checkbox (or `Space` on the selected row) | Hide or show the object. One uniform (`u_o<id>_visible`): no recompile, no relink, and showing it again gives the same image. A hidden object is not traced and not coloured; the trace box stays the same. |
| Click a row, arrow keys | Select that object alone. The viewport highlights it (below). |
| `Ctrl`+click a row | Add it to the selection, or drop it when it is selected. The dock keeps the order rows were selected in: a boolean takes its operands in that order. The last one selected is the selected object (the Transform dock, the gizmo, the highlight). |
| `Shift`+click a row | Select the rows between the current one and it, added in row order. |
| Left click in the viewport | Select the object under the cursor: the nearest along the view ray where objects overlap on screen. A click on the background clears the selection. Its row is selected in this dock. Hidden objects are not picked. |

The selected object is tinted toward amber (mix 0.45 with (1.0, 0.78, 0.20), before the
1/2.2 gamma) and the other objects are dimmed to 0.45 of their colour; with nothing
selected every object has its own colour. Selecting sets one uniform (`u_selected`): no
recompile, no relink.
| `F2`, a double click on the name, or `Edit ▸ Rename Object` | Edit the object's name in its row; `Enter` renames it, `Esc` keeps the old one. An empty name is refused. The window title and the Parameters dock follow. No recompile. |
| `Delete` (`QKeySequence::Delete`, by meaning) with the dock focused, or `Edit ▸ Delete Object` | Remove the selected object. The scene is lowered and linked again (one link); the camera stays. The other objects keep their parameter edits. Deleting the last one leaves the viewport on the background. |

The swatch is the object's colour with the shader's 1/2.2 gamma: a lit face of the object is
about that colour. Colours follow the order objects were made in a scene: the first is
DualC's grey, then orange, blue, green, yellow, purple, teal, pink. `File ▸ Open…` starts a
new scene, so ids and colours start again. There is no colour picker yet: edit the
object's `colour` in the Parameters dock, and the swatch follows. Unchecking `visible`
there (0) checks the box off too.

## Booleans

Select two objects or more in the Objects dock (`Ctrl`+click, in order), then pick an
entry of the `Operations` menu. The result is a new object, named `<op> <id>`, over copies
of the operands, each in its own pose. The operands are hidden, not deleted. The new
object is selected once its shader links.

| Entry | DualC op | Result over `a, b` |
|---|---|---|
| `Union` | `union` | `a` or `b` |
| `Intersection` | `intersection` | `a` and `b` |
| `Difference` | `difference` | `a` minus `b`: the first selected minus the second |
| `Xor` | `xor` | in one but not both |
| `Smooth Union` | `smooth-union` | `union` blended over the width `k` |
| `Smooth Intersection` | `smooth-intersection` | `intersection` blended over `k` |
| `Smooth Difference` | `smooth-difference` | `difference` blended over `k` |

- **More than two operands** fold left in selection order: `a, b, c` gives
  `op(op(a, b), c)`, so `Difference` cuts `b` and `c` out of `a`.
- **`Blend k`** is in millimetres, 0.25 by default (DualC's default), from 0.0001 to
  100000, four decimals, like every length of the operation panels.
  It is read when the entry is picked; changing it later does not change a result.
  The sharp booleans ignore it.
- **Any object** can be an operand: a primitive, a TPMS or lattice graph, an imported
  `[mesh]` or `[winding]`, the result of an earlier boolean.
- **Non-metric operands (E-10).** A smooth boolean blends distances, so it misbehaves on a
  field that is not one: a `[winding]` mesh, a raw TPMS (`gyroid`, …), or a `twist`,
  `bend` or `displace`, with no `normalize` above them. Such an operand goes under
  `normalize` in the result's graph. The undo step says so, as
  `smooth-union 7 (normalize on bunny.stl)`, and the log and status bar read
  `smooth-union of bunny.stl, ball.json, k 0.25 mm; normalize on bunny.stl, not distance
  fields (E-10)`. A sharp boolean takes every operand as it is.

## Fills

Select one object, the volume, then pick `Operations ▸ Fill with TPMS…` or
`Fill with Lattice…`. A dialog sets the fill; `OK` makes a new object, named
`<type> fill <id>`, and hides the volume. The new object is selected once its shader links.
The log and status bar read `octet fill of bunny.stl, wavelength 3.81 mm, radius 0.381 mm,
node radius 0.381 mm`.

| Dialog | Graph |
|---|---|
| `Fill with TPMS…` | `intersection(onion(normalize(tpms), thickness=t), volume)` |
| `Fill with Lattice…` | `intersection(lattice, volume)` |

| Field | Unit | Default |
|---|---|---|
| `Type` | | `gyroid` (TPMS: `gyroid`, `schwarz-p`, `diamond`, `fischer-koch`, `lidinoid`, `neovius`, DualC's `tpmsKinds`), `octet` (lattice: `sc`, `bcc`, `fcc`, `octet`, DualC's `strutKinds`). The tooltip of each says what it is. |
| `Wavelength` | mm | A tenth of the diagonal of the volume's trace box (its bounds padded 5% a side, posed), three significant digits: about ten cells across, as DualC's `dualc_lattice`. |
| `Wall thickness t` (TPMS) | mm | Wavelength / 5. Onion's `t` on the normalized TPMS: the wall is `2 t` across. |
| `Radius` (lattice) | mm | Wavelength / 10, the radius of a strut at mid-span. |
| `Node radius` (lattice) | mm | The radius. Larger, it tapers the struts, fat at the nodes (the costly P12 shape). |
| `Keep the skin` | mm | Off. On, the thickness starts at the wall (`2 t`) or strut (`2 × radius`) width. |

- **The cells are centred** on the volume's trace box: the TPMS phase origin, or a
  lattice cell's centre.
- **Keep the skin** adds a union with an onion of the volume:
  `union(fill, onion(offset(volume, r=-s/2), thickness=s/2))`, the volume's inside within
  `s` of its surface. The part keeps its size. A `[winding]` volume goes under `normalize`
  there, since an onion reads distances.
- **The result is an object like any other:** its parameters (wavelength, thickness,
  radii) are in the Parameters dock, and a boolean takes it as an operand. A fill of a
  `[mesh]` volume is metric; a fill of a `[winding]` one is not, so a smooth boolean wraps
  it in `normalize` (E-10).

## Operation list

The `Operations` menu and the fill dialogs are generated from DualC's registries in
`example_common` (P05 T3): the booleans `BoolOp` names, the TPMS of `tpmsKinds()` and the
crystals of `strutKinds()`, in DualC's order. A new DualC op of one of these families
appears in the menu or in the `Type` combo, with its fields, without an editor change. Each
family's parameters are the editor's (DualC's registries carry no parameter list); every
one is a length in mm. `ctest -R operations` fails when an op below is missing here.

| Op | Family | Where | Parameters (default) |
|---|---|---|---|
| `union` | boolean | `Operations ▸ Union` | none |
| `intersection` | boolean | `Operations ▸ Intersection` | none |
| `difference` | boolean | `Operations ▸ Difference` | none |
| `xor` | boolean | `Operations ▸ Xor` | none |
| `smooth-union` | smooth boolean | `Operations ▸ Smooth Union` | `Blend k` (0.25) |
| `smooth-intersection` | smooth boolean | `Operations ▸ Smooth Intersection` | `Blend k` (0.25) |
| `smooth-difference` | smooth boolean | `Operations ▸ Smooth Difference` | `Blend k` (0.25) |
| `gyroid` | TPMS | `Fill with TPMS…` | `Wavelength` (box diagonal / 10), `Wall thickness t` (wavelength / 5) |
| `schwarz-p` | TPMS | `Fill with TPMS…` | as `gyroid` |
| `diamond` | TPMS | `Fill with TPMS…` | as `gyroid` |
| `fischer-koch` | TPMS | `Fill with TPMS…` | as `gyroid` |
| `lidinoid` | TPMS | `Fill with TPMS…` | as `gyroid` |
| `neovius` | TPMS | `Fill with TPMS…` | as `gyroid` |
| `sc` | strut lattice | `Fill with Lattice…` | `Wavelength` (box diagonal / 10), `Radius` (wavelength / 10), `Node radius` (the radius) |
| `bcc` | strut lattice | `Fill with Lattice…` | as `sc` |
| `fcc` | strut lattice | `Fill with Lattice…` | as `sc` |
| `octet` | strut lattice | `Fill with Lattice…` | as `sc` |

The menu text is the token in title case (`smooth-union` gives `Smooth Union`). Both fill
dialogs add `Keep the skin` (see *Fills*), which is the editor's, not an op parameter.

## Export

`File ▸ Export Selected Object…` (`Ctrl+E`, E for Export, the same on AZERTY) contours the
selected object; `File ▸ Export Visible Scene…` (`Ctrl+Shift+E`) the union of the visible
objects. Each object is posed in world mm and carries its parameter edits, so the file is
what the viewport shows. The dialog (`../design/08-export.md`):

| Field | Default | Effect |
|---|---|---|
| `File` + `Browse…` | The last export's folder and extension, else the scene file's folder (or Documents) and `.stl`; named after the object, or `scene` | The extension picks the format: `.stl` binary STL, `.3mf` 3MF (1 unit = 1 mm), `.obj` OBJ with per-vertex normals, case-insensitive. Any other extension greys out `Export`. |
| `Format` | — | The format the extension names, or how to fix it. |
| `Depth` | 7 (3–12) | DualC's octree depth: 2^depth cells across the bounds. Each level is about four times the triangles. |
| `Estimate` | — | `about N triangles, M RAM, cell C mm`, from a coarse contour at depth min(depth − 1, 5) grown four times per level; RAM at DualC's 3 KiB per triangle of one contour (the whole mesh, or the busiest tile). On the P06 test case it was 0.89× the real count. The probe runs on the GUI thread, once per bounds and probe depth. |
| `Bounds min / max` + `Reset` | The field's own bounds padded by 5% (the trace box) | The box DualC samples, x y z in mm. An empty box greys out `Export`. |
| `Tiled, one tile in RAM` + `tile depth` | off; tile depth = depth − 2 (2 to depth − 1) | STL and 3MF only (greyed out for OBJ). DualC's streaming writer contours tile by tile, so peak RAM is one tile; seam vertices are duplicated, so the file is watertight for a slicer but not vertex-welded. The estimate then names the tiles per axis. Closes E-27. |

`Export` starts the job on a worker thread. A window-modal progress dialog shows the stage
(`Sampling`, `Contouring`, `Writing`, or `Tile i of n`) while the viewport keeps drawing.
`Cancel` stops DualC at its next checkpoint: **no file is left**, not even a `.part`, and a
file already at that path is untouched (DualC writes `<file>.part` and renames it only on
success). The log then gets `export <file>: depth D in T ms: N triangles, watertight` (or
the open and non-manifold edge counts DualC reports; a tiled export reports its tile depth),
and an error goes to the status bar and the log.

## Undo and redo

Every change to the scene is one step on an undo stack (`../design/04-scene-model.md`):

| Step | Undo |
|---|---|
| Add (`File ▸ Add…`, a drop, a later command-line graph, an imported mesh) | Removes the object. Its id is not given to the next add. |
| `Edit ▸ Make Watertight` | The `[winding]` object comes back. |
| A boolean or a fill (`Operations` menu) | Removes the result and shows the operands, or the volume, that were shown. |
| Delete | Puts the object back at its place, with its id, colour, name and parameter edits. |
| Rename, show or hide, an edit of `colour` or `matrix` in the Parameters dock | The old value; one uniform, no recompile, no relink. |
| A parameter edit | The old value; one uniform. Steps of one spin box in a row are one step. |
| A reload of a changed file | The graph before the reload, with its edits. Several files reloaded together are one step. |

An undo or redo that adds or removes an object, or changes a graph, lowers and links the
scene again, as a delete does; the camera stays. `File ▸ Open…` starts a new scene and
clears the history. If the scene a step leads to cannot be shown (a field-graph or shader
error), the step is undone again, the error goes to the status bar and the log, and
`Redo` offers it back. The selection is not undone.

## Parameters

The Parameters dock lists the binding table of the scene's shader (`GlslScene::bindings`):
every value that the generated shader reads from a uniform. They are grouped under their
object's name, then under their node, shown as its op and its pointer with the object's in
front (`onion /objects/1/root/in/0/in/1`). **Like the Transform dock, it follows the
selection:** with an object selected (a click in the viewport or in the Objects dock) only
that object's section is listed; with no selection (`Esc`, or a click on empty space) every
object's is, one after the other. The scroll bar stays for a long object. Each object ends with its own `object
/objects/<id>` group: `matrix` (the object's local-from-world transform), `visible` (1 or 0)
and `colour` (linear RGB), then `boxMin` and `boxMax`, the object's world box that the
culling reads when the object is costly (greyed out: an edit of `matrix` moves them, nothing else edits them;
`../design/03-scene-lowering.md` § Culling). Each row is the parameter's name
(`thickness`, `wavelength`, `center`, `mat`). Its tooltip gives the uniform's type and name.

| Type | Editor |
|---|---|
| `float` | One spin box. |
| `vec3` | Three spin boxes: x, y, z. |
| `mat4` | A 4×4 grid, row-major, as the binding stores it. `translate`, `rotate` and `transform` nodes bind the local-from-world matrix. |

Using a spin box:

| Input | Effect |
|---|---|
| Up and down arrows, the wheel, the arrow buttons | Step the value and apply it at once. The step is a tenth of the value's magnitude (0.1 for 1.0, 0.01 for 0.3). Holding the key speeds it up. |
| Typing | The value applies on `Enter` or when the box loses focus, not at every character. |
| Page Up and Page Down | Ten steps. |

The range is −10⁶ to 10⁶, with up to six decimals. No other bound is enforced: a zero
wavelength or thickness gives what the shader computes.

An edit only sets the uniform. The shader is not recompiled or relinked, so it shows at once.
Each edit is an undo step (*Undo and redo* above).
The view previews and settles as for a camera move. Edits are not written to the file. A
reload of the file, or opening another one, brings back the file's values and rebuilds the
dock. Adding or deleting another object keeps them. The trace step sizes stay those of the file's values: they come from the compiled graph,
as in `dualc_field_view`. A wavelength much smaller than the file's can trace with visible
artefacts. Save it in the file to recompute them.

Keys typed into a spin box go to it, not to the viewport. Click the viewport to give it back
the keys (`F`).

## Section

A section cuts the view at a plane normal to one axis and shows the inside: the thin
walls of a TPMS, the struts of a lattice, the skin of a fill. It is the trace shader's
`uClip` planes (DualC's own viewers have the same), so it is viewing only: the field, the
pick, the scene file and the export never see it.

| Control | Effect |
|---|---|
| `X`, `Y` or `Z` button (toolbar or `View ▸ Section`) | Cut at a plane normal to that axis, through the middle of the trace box, keeping the low side. One axis at a time: pressing another moves the cut to it; pressing the lit one turns the section off. |
| `−`, `+` buttons, or the keys | Move the plane a twentieth of the trace box along its axis, toward its low or high end, clamped to the box. Greyed out with no section on. |
| Label | The plane's position along its axis, in scene units (`x = 1.20 mm`), or `off`. |

Every change is a uniform, so there is no compile; the view previews and settles as for a
camera move. The cut face is lit like any surface. Turning the section off restores the
exact image. The section is kept across a reload, an add and a `.dcscene` open, and is not
saved.

## Timing

| Field | Measured as |
|---|---|
| `compile` | Wall time of the GL compile of both shader stages plus the program link, for the scene on screen. DualC's parse and codegen are not included. It also goes to the Log at each load. |
| `frame` | GPU time of the last full-resolution (settled) frame, from a `GL_TIME_ELAPSED` query. |
| `preview` | GPU time of the last preview frame, with the upscale blit. |
| `mesh grid` | Shown when the scene on screen has a `mesh` or `winding` node: the grid each is previewed at, samples per axis (`96³`). |

The query is read a few milliseconds after the frame, without stalling. While idle, nothing
is measured and the values stay.

## Transform

The selected object is moved, rotated and scaled from the viewport (the gizmo) or from the
Transform dock. Lengths are millimetres, the unit DualC's graphs are written in
(`"units": "mm"`). A pose is the object's rigid transform and one uniform scale factor about
its origin. Neither the gizmo nor the dock compiles anything: the object's matrix, scale,
culling box and the scene's trace box (`uBoundsMin`, `uBoundsMax`) are uniforms. The pose is
saved in the `.dcscene` (`transform`, `scale`).

### Gizmo

With an object selected, the viewport draws its world axes at its origin: x red, y green,
z blue. The keys start a modal drag from where the cursor is:

| Key | Drag |
|---|---|
| `G` | Move. Free: the origin follows the cursor in the plane facing the camera. |
| `R` | Rotate about the view direction, by the angle the cursor sweeps around the origin. |
| `S` | Scale, uniform: the factor is the ratio of the cursor's distance to the origin now to its distance at the start. |
| `X`, `Y`, `Z` (during a drag) | Constrain the move or the rotation to that world axis, drawn across the view. The same key again frees it. A scale has no axis. |
| Left click, `Return`, `Enter` | Apply: one undo step, `Move <name>`, `Rotate <name>` or `Scale <name>`. |
| `Esc`, right click | Cancel: the object goes back to where it was. |

During a drag the bottom-left of the viewport reads the change (`Move x: 12.50 mm`,
`Rotate z: 90.0°`, `Scale: 1.250`). The camera does not move during a drag. Keys are matched
by the character they type, so `G`, `R`, `S`, `X`, `Y` and `Z` are the keys labelled so on any
layout, Belgian AZERTY included. `Ctrl+S` is still Save Scene: it types no `s`.

### Transform dock

| Row | Boxes | Unit, range |
|---|---|---|
| Position | x, y, z | mm, 3 decimals, ±1e6 |
| Rotation | x, y, z | degrees, 2 decimals, −360 to 360, wrapping. The rotation is Rz · Ry · Rx about the world axes. |
| Scale | factor | 4 decimals, 1e-4 to 1e4, step 0.1 |

A value is taken on `Return`, when the box loses focus, or at each step of its arrows, and is
one undo step, `Transform <name>`. A component that was not typed keeps its exact value, so
typing x does not round y, z or the rotation. The boxes are greyed out with nothing selected.
The Parameters dock's `matrix` and `s` rows of the object edit the same pose.

## Mouse and keys

The same semantics as DualC's viewers (`OrbitState` in `examples/raymarch_gl`), plus pan.

| Input | Action |
|---|---|
| Left-drag | Orbit. Yaw −0.008 rad per pixel right, pitch +0.008 rad per pixel down. Pitch is clamped to ±1.5 rad. A drag starts once the cursor is the platform's drag distance (`QApplication::startDragDistance()`, 10 px by default, Manhattan) from the press; the whole movement then counts. |
| Left click (press and release within the drag distance, no Shift) | Select the object under the cursor, or clear the selection on the background (§ Objects). The view does not move. |
| Wheel | Dolly. Distance × 0.9 per notch away from you (closer), ÷ 0.9 per notch toward you. Floor 1e-4. |
| Middle-drag, or Shift + left-drag | Pan (from the same drag distance). The target slides in the view plane, and the point at the target follows the cursor. Yaw, pitch and distance are kept. |
| `G`, `R`, `S`, `X`, `Y`, `Z`, `Esc` | The transform gizmo, with an object selected (§ Transform). |
| `F` (either case) | Frame the bounds (does nothing while no graph is on screen). The target goes to the trace box centre and the distance to 1.3 × its diagonal. Yaw and pitch are kept. |
| `+`, `−` (main row or keypad) | Move the section plane, when one is on (§ Section). Window shortcuts: they work wherever the focus is. |

Keys are matched by the character they type (`QKeyEvent::text()`), not by key position.
`F` is the key labelled F on any layout, Belgian AZERTY included. The viewport takes
keyboard focus when clicked.

### Preview and settle

While a drag is held, and for 0.18 s after the last camera change or button release, frames
are traced at `--preview-scale`. Then one full-resolution frame is drawn. After that the
viewport draws nothing until the next input, a resize or an expose. Holding a drag still
keeps the preview frame on screen and draws nothing new.

## Measurement run

`DualCEditor --bench [--preview-scale S] [--bench-size WxH] [--bench-frames N] [graph]`
shows a bare viewport (no menu, docks or status bar) whose traced size is `WxH` device
pixels, then runs two phases of `N` frames each:

| Phase | Frames |
|---|---|
| `moving` | The camera orbits one drag pixel per presented frame, so the view previews at `S`, as under a held drag. |
| `settled` | Full-resolution frames of a still camera, one per presented frame. |

It prints one JSON object, then exits 0:

| Field | Meaning |
|---|---|
| `moving.gpuMs`, `settled.gpuMs` | Median `GL_TIME_ELAPSED` time of a frame. A preview frame includes its upscale. |
| `moving.wallMs`, `settled.wallMs` | Median time between presented frames (`frameSwapped`): the trace, Qt's composition and the swap. |
| `…gpuFrames`, `…wallFrames`, `…traced` | Samples behind each median, and the size traced. |
| `compileMs` | GL compile + link of the trace shader. Drivers cache shaders on disk: on llvmpipe the tapered octet linked in 7.2 s the first time and 0.95 s from Mesa's cache. |
| `firstFrameMs` | Wall time from showing the window to the first presented frame. Some drivers finish compiling at the first draw, so it can be much longer than `compileMs`. |
| `glRenderer`, `glVersion`, `qt`, `version` | What ran it. On NVIDIA, `glVersion` carries the driver version. |

Exit 1 when the shader does not link or a phase takes over 60 s, 2 when the graph does not
load, 77 with no GL 3.3 core context (on Linux with no display, as for `--snapshot`).
`scripts/measure_viewport.py` runs it over the P00 reference scenes, and with `--objects`
over P01's scenes of 1 to 10 objects and P07's column stacked in depth (it then also
prints the ms per added object). With `--meshes` it runs scenes of 1, 3 and 5 imported
closed meshes (UV spheres written as OBJ, each a `mesh` node at the 96³ grid) at 0.35 and
1.0 (P04 T2). With `--fills` it runs one imported UV sphere filled with an octet at the fill
dialog's defaults, beside the P00 tapered octet, at 0.35 and 1.0 (P05 T2). `--unit ID` sets
the unit id printed in each row (default `P00.5`, `P01.4` with `--objects`, `P04.2` with
`--meshes`, `P05.2` with `--fills`). Its checklists are in
`docs/roadmap/02-measurements.md`.

```bash
./build/bin/DualCEditor --bench --preview-scale 0.35 part.json
# {"compileMs":79.41,"firstFrameMs":…,"moving":{"gpuFrames":60,"gpuMs":17,…},"settled":{…},…}
```

## Paint log

One line per `paintGL`, off by default:

```bash
QT_FORCE_STDERR_LOGGING=1 QT_LOGGING_RULES="dce.viewport.paint.debug=true" ./build/bin/DualCEditor
# paintGL #1 preview 478x330 65.7 ms
# gpu preview 478x330 65.0 ms
# paintGL #4 full 956x661 110.4 ms
# gpu full 956x661 107.8 ms
```

Each `paintGL` line gives the frame kind, the size actually traced, and the time of the frame.
Each `gpu` line is the `GL_TIME_ELAPSED` time of the same frame, logged when the query is read. While
the log is on, each frame ends with `glFinish`, so the time includes the GPU. Use it to
check that an idle window prints nothing. `QT_FORCE_STDERR_LOGGING=1` is needed when stderr
is not a terminal, because Qt otherwise sends the log to journald.
