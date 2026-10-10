#!/usr/bin/env python3
"""P00 T5: time the viewport on the three reference scenes, ready for the table
in docs/roadmap/02-measurements.md. The owner runs it on the GTX 1050 Ti laptop.

  python scripts/measure_viewport.py                    # Windows, or Linux on its default GPU
  python scripts/measure_viewport.py --prime            # Linux PRIME: offload to the NVIDIA GPU
  python scripts/measure_viewport.py --field-view PATH  # also diff snapshots against dualc_field_view

Each scene runs at preview scales 0.35, 0.5 and 1.0 through `DualCEditor --bench`
(docs/command_reference/01-viewport.md). The script prints one markdown row per run and
writes the raw JSON lines to measurements-<date>.jsonl in the working directory.
"""
from __future__ import annotations

import argparse
import datetime
import json
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


def git_commit() -> str:
    try:
        rev = subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=ROOT,
                             capture_output=True, text=True, check=True).stdout.strip()
        dirty = subprocess.run(["git", "status", "--porcelain", "--untracked-files=no"],
                               cwd=ROOT, capture_output=True, text=True).stdout.strip()
        return rev + ("+dirty" if dirty else "")
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


def run_bench(exe: Path, graph: Path, scale: float, size: str, frames: int,
              env: dict) -> dict:
    cmd = [str(exe), "--bench", "--bench-size", size, "--bench-frames", str(frames),
           "--preview-scale", str(scale), str(graph)]
    proc = subprocess.run(cmd, capture_output=True, text=True, env=env, timeout=600)
    if proc.returncode != 0:
        raise RuntimeError(f"{' '.join(cmd)} exited {proc.returncode}: {proc.stderr.strip()}")
    return json.loads(proc.stdout.strip().splitlines()[-1])


def ms(v) -> str:
    return "-" if v is None else f"{v:.1f}"


def row(date: str, scene: str, r: dict, size: str, where: str) -> str:
    mv, st = r["moving"], r["settled"]
    moving = f"{ms(mv['gpuMs'])} GPU / {ms(mv['wallMs'])} wall ({mv['traced'][0]}×{mv['traced'][1]})"
    settled = f"{ms(st['gpuMs'])} GPU / {ms(st['wallMs'])} wall"
    return (f"| {date} | P00.5 | {scene} | {size.replace('x', '×')} | {r['previewScale']:g} "
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
    ap.add_argument("--keep", action="store_true", help="keep the scratch folder")
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
    rows, first = [], None
    try:
        todo = scenes(args.samples, work)
        with jsonl.open("a") as out:
            for name, graph in todo:
                for scale in SCALES:
                    print(f"{name} at {scale:g} ...", file=sys.stderr, flush=True)
                    r = run_bench(args.exe, graph, scale, args.size, args.frames, env)
                    r.update(scene=name, date=date, commit=git_commit(),
                             os=platform.platform(), prime=args.prime)
                    out.write(json.dumps(r) + "\n")
                    first = first or r
                    where = f"{r['commit']}, {platform.system()} {platform.release()}, {r['glVersion']}"
                    rows.append(row(date, name, r, args.size, where))
        print(f"\nGPU: {first['glRenderer']}, GL {first['glVersion']}, Qt {first['qt']}")
        print(f"Raw results appended to {jsonl}\n")
        print("\n".join(rows))
        if args.field_view:
            print("\nSnapshots, editor against dualc_field_view (800×600, default view):")
            for name, graph in todo:
                print(compare(args.exe, args.field_view, name, graph, work, env))
            args.keep = True
    finally:
        if not args.keep:
            shutil.rmtree(work, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
