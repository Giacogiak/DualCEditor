#!/usr/bin/env python3
"""P00 T5 and P01 T4: time the viewport on reference scenes, ready for the tables
in docs/roadmap/02-measurements.md. The owner runs it on the GTX 1050 Ti laptop.

  python scripts/measure_viewport.py                    # Windows, or Linux on its default GPU
  python scripts/measure_viewport.py --prime            # Linux PRIME: offload to the NVIDIA GPU
  python scripts/measure_viewport.py --field-view PATH  # also diff snapshots against dualc_field_view
  python scripts/measure_viewport.py --objects          # P01 T4, P07: frame time against object count
  python scripts/measure_viewport.py --objects --unit P07.3   # label the rows with another unit

Without --objects, each P00 scene runs at preview scales 0.35, 0.5 and 1.0. With
--objects, scenes of 1, 3, 5 and 10 analytic primitives side by side, of 1, 3, 5 and 10
stacked in depth (P07 T1), and of 1 and 3 TPMS-in-box objects run at 0.35 and 1.0, and the
script also prints the growth per added object. Every run goes
through `DualCEditor --bench` (docs/command_reference/01-viewport.md). The script prints
one markdown row per run and appends the raw JSON lines to measurements-<date>.jsonl in the
working directory.
"""
from __future__ import annotations

import argparse
import datetime
import json
import math
import os
import platform
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCALES = (0.35, 0.5, 1.0)
# DualC docs/command_reference/12-dualc_field_view/02-nodes-and-examples.md, recipe P12.
TAPERED_OCTET = ("intersection(box(min=[-1,-1,-1],max=[1,1,1]),"
                 "octet(wavelength=0.5,radius=0.03,nodeRadius=0.08))\n")
# DualC's dualc_gen_demo cube (examples/demo_meshes.cpp makeCube, side 1), which
# mesh_lattice.json names as cube.obj. DualC generates it at build time only.
CUBE_OBJ = ("v -0.5 -0.5 -0.5\nv 0.5 -0.5 -0.5\nv -0.5 0.5 -0.5\nv 0.5 0.5 -0.5\n"
            "v -0.5 -0.5 0.5\nv 0.5 -0.5 0.5\nv -0.5 0.5 0.5\nv 0.5 0.5 0.5\n"
            "f 1 3 4\nf 1 4 2\nf 5 6 8\nf 5 8 7\nf 1 2 6\nf 1 6 5\n"
            "f 3 7 8\nf 3 8 4\nf 1 5 7\nf 1 7 3\nf 2 4 8\nf 2 8 6\n")
# P01 T4. Primitives sit in a fixed 5x2 grid of slots, 1.2 apart, alternately a sphere of
# radius 0.4 and a box of side 0.8, so no two overlap. The view frames the union, so the
# objects cover about the same share of the screen whatever their count.
PRIMITIVE_COUNTS = (1, 3, 5, 10)
# P07 T1. The same primitives in a column along the default view axis (yaw 0.7, pitch 0.5,
# from the target toward the camera), 1.2 apart, the first nearest the camera: the rays near
# the front object meet every box behind it, the case per-object culling must still serve.
# The framing grows with the column, so the front object shrinks as objects are added.
VIEW_AXIS = (math.cos(0.5) * math.sin(0.7), math.sin(0.5), math.cos(0.5) * math.cos(0.7))
TPMS_COUNTS = (1, 3)
OBJECT_SCALES = (0.35, 1.0)
# gyroid_box.json without its spherical hole; copies sit 5 apart along x.
TPMS_IN_BOX = ("intersection(box(min=[-2,-2,-2],max=[2,2,2]),"
               "onion(normalize(gyroid(wavelength=1)),thickness=0.3))")
PRIME_ENV = {"__NV_PRIME_RENDER_OFFLOAD": "1", "__GLX_VENDOR_LIBRARY_NAME": "nvidia"}


def default_exe() -> Path:
    name = "DualCEditor.exe" if os.name == "nt" else "DualCEditor"
    for d in ("build/bin", "build/bin/Release", "build/Release/bin"):
        if (ROOT / d / name).exists():
            return ROOT / d / name
    return ROOT / "build/bin" / name


def scenes(samples: Path, work: Path) -> list[tuple[str, Path]]:
    octet = work / "tapered_octet.fld"
    octet.write_text(TAPERED_OCTET)
    lattice = work / "mesh_lattice.json"
    shutil.copy(samples / "mesh_lattice.json", lattice)
    (work / "cube.obj").write_text(CUBE_OBJ)
    return [("gyroid_box.json", samples / "gyroid_box.json"),
            ("tapered octet (P12)", octet),
            ("mesh_lattice.json", lattice)]


def primitive(i: int, depth: bool = False) -> str:
    if depth:
        c = tuple(round(-1.2 * i * a, 4) for a in VIEW_AXIS)
    else:
        c = (1.2 * (i % 5 - 2), 1.2 * (0.5 - i // 5), 0)
    if i % 2 == 0:
        return f"sphere(center=[{','.join(f'{v:g}' for v in c)}],radius=0.4)\n"
    lo = ",".join(f"{v - 0.4:g}" for v in c)
    hi = ",".join(f"{v + 0.4:g}" for v in c)
    return f"box(min=[{lo}],max=[{hi}])\n"


def object_scenes(work: Path) -> list[tuple[str, int, list[Path]]]:
    """(name, object count, graphs) for P01 T4 and P07: one graph file per object."""
    prims, depth, tpms = [], [], []
    for i in range(max(PRIMITIVE_COUNTS)):
        prims.append(work / f"primitive_{i}.fld")
        prims[-1].write_text(primitive(i))
        depth.append(work / f"depth_{i}.fld")
        depth[-1].write_text(primitive(i, depth=True))
    for i in range(max(TPMS_COUNTS)):
        tpms.append(work / f"tpms_box_{i}.fld")
        tpms[-1].write_text(f"translate(by=[{5 * i - 5 * (max(TPMS_COUNTS) // 2)},0,0],"
                            f"{TPMS_IN_BOX})\n")
    return ([(f"{n} primitive{'s' if n > 1 else ''}", n, prims[:n]) for n in PRIMITIVE_COUNTS]
            + [(f"{n} stacked in depth", n, depth[:n]) for n in PRIMITIVE_COUNTS]
            + [(f"{n} TPMS-in-box", n, tpms[:n]) for n in TPMS_COUNTS])


def growth(results: list[tuple[str, int, dict]], kind: str, scale: float) -> str:
    """Least-squares ms per added object, per phase, over one family of scenes."""
    pts = [(n, r) for name, n, r in results if kind in name and abs(r["previewScale"] - scale) < 1e-3]
    if len(pts) < 2:
        return ""
    out = []
    for phase in ("moving", "settled"):
        for key in ("gpuMs", "wallMs"):
            xy = [(n, r[phase][key]) for n, r in pts if r[phase][key] is not None]
            if len(xy) < 2:
                continue
            mx = sum(x for x, _ in xy) / len(xy)
            my = sum(y for _, y in xy) / len(xy)
            slope = (sum((x - mx) * (y - my) for x, y in xy)
                     / sum((x - mx) ** 2 for x, _ in xy))
            out.append(f"{phase} {key[:-2]} {slope:+.2f} ms")
    counts = ", ".join(str(n) for n, _ in pts)
    return f"{kind} at {scale:g} ({counts} objects), per added object: " + "; ".join(out)


def git_commit() -> str:
    try:
        rev = subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=ROOT,
                             capture_output=True, text=True, check=True).stdout.strip()
        dirty = subprocess.run(["git", "status", "--porcelain", "--untracked-files=no"],
                               cwd=ROOT, capture_output=True, text=True).stdout.strip()
        return rev + ("+dirty" if dirty else "")
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


def run_bench(exe: Path, graphs: list[Path], scale: float, size: str, frames: int,
              env: dict) -> dict:
    cmd = [str(exe), "--bench", "--bench-size", size, "--bench-frames", str(frames),
           "--preview-scale", str(scale), *map(str, graphs)]
    proc = subprocess.run(cmd, capture_output=True, text=True, env=env, timeout=600)
    if proc.returncode != 0:
        raise RuntimeError(f"{' '.join(cmd)} exited {proc.returncode}: {proc.stderr.strip()}")
    return json.loads(proc.stdout.strip().splitlines()[-1])


def ms(v) -> str:
    return "-" if v is None else f"{v:.1f}"


def row(date: str, unit: str, scene: str, r: dict, size: str, where: str) -> str:
    mv, st = r["moving"], r["settled"]
    moving = f"{ms(mv['gpuMs'])} GPU / {ms(mv['wallMs'])} wall ({mv['traced'][0]}×{mv['traced'][1]})"
    settled = f"{ms(st['gpuMs'])} GPU / {ms(st['wallMs'])} wall"
    return (f"| {date} | {unit} | {scene} | {size.replace('x', '×')} | {r['previewScale']:g} "
            f"| {moving} | {settled} | {ms(r['compileMs'])} | {where} |")


# --- snapshot comparison against dualc_field_view (8-bit RGB/RGBA PNGs) ---

def read_png(path: Path) -> tuple[int, int, int, bytes]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path}: not a PNG")
    pos, idat, w = 8, b"", 0
    while pos < len(data):
        n, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        pos += 12 + n
        if kind == b"IHDR":
            w, h, depth, ctype, _, _, interlace = struct.unpack(">IIBBBBB", body)
            if depth != 8 or ctype not in (2, 6) or interlace:
                raise ValueError(f"{path}: only 8-bit RGB/RGBA, non-interlaced")
            bpp = 3 if ctype == 2 else 4
        elif kind == b"IDAT":
            idat += body
    raw, stride = zlib.decompress(idat), w * bpp
    out, prev = bytearray(), bytearray(stride)
    for y in range(h):
        f, line = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b, c = prev[i], prev[i - bpp] if i >= bpp else 0
            if f == 1:
                line[i] = (line[i] + a) & 255
            elif f == 2:
                line[i] = (line[i] + b) & 255
            elif f == 3:
                line[i] = (line[i] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        out += line
        prev = line
    return w, h, bpp, bytes(out)


def diff_png(a: Path, b: Path) -> str:
    wa, ha, ba, pa = read_png(a)
    wb, hb, bb, pb = read_png(b)
    if (wa, ha) != (wb, hb):
        return f"sizes differ: {wa}×{ha} against {wb}×{hb}"
    differ, worst = 0, 0
    for i in range(wa * ha):
        d = max(abs(pa[i * ba + k] - pb[i * bb + k]) for k in range(3))
        if d:
            differ += 1
            worst = max(worst, d)
    return f"{differ} / {wa * ha} px differ, max channel diff {worst}"


def compare(exe: Path, field_view: Path, name: str, graph: Path, work: Path, env: dict) -> str:
    stem = graph.stem
    ours, theirs = work / f"{stem}_editor.png", work / f"{stem}_field_view.png"
    subprocess.run([str(exe), "--snapshot", str(ours), str(graph)], env=env, check=True,
                   capture_output=True, timeout=300)
    # dualc_field_view resolves a relative mesh path in the working directory.
    subprocess.run([str(field_view), str(graph), "--snapshot", str(theirs)], env=env,
                   cwd=graph.parent, check=True, capture_output=True, timeout=300)
    return f"{name}: {diff_png(ours, theirs)} (PNGs in {work})"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--exe", type=Path, default=default_exe(), help="DualCEditor binary")
    ap.add_argument("--samples", type=Path,
                    default=ROOT / "build/_deps/dualc-src/examples/samples",
                    help="DualC's examples/samples folder")
    ap.add_argument("--size", default="1280x800", help="traced size, WxH device pixels")
    ap.add_argument("--frames", type=int, default=60, help="frames timed per phase")
    ap.add_argument("--prime", action="store_true",
                    help="Linux: offload to the NVIDIA GPU (PRIME render offload)")
    ap.add_argument("--field-view", type=Path,
                    help="dualc_field_view binary: also diff 800×600 snapshots against it")
    ap.add_argument("--objects", action="store_true",
                    help="P01 T4, P07: scenes of 1, 3, 5, 10 primitives side by side and "
                         "stacked in depth, and of 1, 3 TPMS-in-box")
    ap.add_argument("--keep", action="store_true", help="keep the scratch folder")
    ap.add_argument("--unit", help="unit id printed in each row (default P00.5, or P01.4 "
                                   "with --objects)")
    args = ap.parse_args()

    env = dict(os.environ)
    if args.prime:
        env.update(PRIME_ENV)
    if not args.exe.exists():
        print(f"no binary at {args.exe}; build first or pass --exe", file=sys.stderr)
        return 1

    work = Path(tempfile.mkdtemp(prefix="dce_measure_"))
    date = datetime.date.today().isoformat()
    jsonl = Path(f"measurements-{date}.jsonl").resolve()
    rows, first, results = [], None, []
    try:
        if args.objects:
            todo = object_scenes(work)
            unit, scales = "P01.4", OBJECT_SCALES
        else:
            todo = [(name, 1, [graph]) for name, graph in scenes(args.samples, work)]
            unit, scales = "P00.5", SCALES
        unit = args.unit or unit
        with jsonl.open("a") as out:
            for name, n, graphs in todo:
                for scale in scales:
                    print(f"{name} at {scale:g} ...", file=sys.stderr, flush=True)
                    r = run_bench(args.exe, graphs, scale, args.size, args.frames, env)
                    r.update(scene=name, objects=n, date=date, commit=git_commit(),
                             os=platform.platform(), prime=args.prime)
                    out.write(json.dumps(r) + "\n")
                    first = first or r
                    results.append((name, n, r))
                    where = f"{r['commit']}, {platform.system()} {platform.release()}, {r['glVersion']}"
                    rows.append(row(date, unit, name, r, args.size, where))
        print(f"\nGPU: {first['glRenderer']}, GL {first['glVersion']}, Qt {first['qt']}")
        print(f"Raw results appended to {jsonl}\n")
        print("\n".join(rows))
        if args.objects:
            print()
            for kind in ("primitive", "stacked in depth", "TPMS-in-box"):
                for scale in scales:
                    print(growth(results, kind, scale))
        if args.field_view and not args.objects:
            print("\nSnapshots, editor against dualc_field_view (800×600, default view):")
            for name, _, (graph,) in todo:
                print(compare(args.exe, args.field_view, name, graph, work, env))
            args.keep = True
    finally:
        if args.keep:
            print(f"Scene files kept in {work}")
        else:
            shutil.rmtree(work, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
