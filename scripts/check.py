#!/usr/bin/env python3
"""DualCEditor gate: what CI runs, and what runs before every commit.

  python scripts/check.py          # docs + hygiene + configure/build/ctest
  python scripts/check.py --fast   # docs + hygiene only, about a second
  python scripts/check.py --build-dir DIR   # build tree (default: build)

Docs contract: every folder under docs/ (and under plan/, whenever a plan
runner's folder exists) has a README.md that names every file and folder
directly inside it (principle 2 in CLAUDE.md).
"""

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
INDEXED_TREES = ["docs"]      # must exist
OPTIONAL_TREES = ["plan"]     # indexed when present
JUNK = ["*.swp", "*.swo", "*.bak", "*.orig", ".DS_Store"]


def check_indexes() -> list[str]:
    problems = []
    for tree in INDEXED_TREES + OPTIONAL_TREES:
        top = ROOT / tree
        if not top.is_dir():
            if tree in INDEXED_TREES:
                problems.append(f"{tree}/ is missing")
            continue
        for folder in [top, *[p for p in top.rglob("*") if p.is_dir()]]:
            readme = folder / "README.md"
            rel = folder.relative_to(ROOT)
            if not readme.is_file():
                problems.append(f"{rel}/README.md is missing")
                continue
            text = readme.read_text(encoding="utf-8")
            for child in sorted(folder.iterdir()):
                if child.name == "README.md":
                    continue
                if child.name not in text:
                    problems.append(f"{rel}/README.md does not name {child.name}")
    return problems


def check_hygiene() -> list[str]:
    tracked = subprocess.run(["git", "ls-files"], cwd=ROOT, capture_output=True,
                             text=True, check=True).stdout.splitlines()
    return [f"junk file tracked: {f}" for f in tracked
            if any(Path(f).match(p) for p in JUNK)]


def run(cmd: list[str]) -> bool:
    print("[gate] $ " + " ".join(cmd), flush=True)
    return subprocess.run(cmd, cwd=ROOT).returncode == 0


def build_and_test(build_dir: str) -> bool:
    configure = ["cmake", "-S", ".", "-B", build_dir, "-DCMAKE_BUILD_TYPE=Release",
                 "-DDUALCEDITOR_BUILD_TESTS=ON"]
    if not (ROOT / build_dir / "CMakeCache.txt").exists():
        configure += ["-G", "Ninja"] if sys.platform != "win32" else []
    return (run(configure)
            and run(["cmake", "--build", build_dir, "--config", "Release", "-j"])
            and run(["ctest", "--test-dir", build_dir, "-C", "Release",
                     "--output-on-failure"]))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--fast", action="store_true", help="docs + hygiene only")
    ap.add_argument("--build-dir", default="build")
    args = ap.parse_args()

    problems = check_indexes() + check_hygiene()
    for p in problems:
        print(f"[gate] FAIL {p}")
    if problems:
        return 1
    print("[gate] docs + hygiene OK")
    if args.fast:
        return 0

    if not build_and_test(args.build_dir):
        print("[gate] FAIL build/test")
        return 1
    print("[gate] all checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
