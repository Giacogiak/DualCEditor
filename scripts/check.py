#!/usr/bin/env python3
"""
Gate: docs contract + repo hygiene check.

Usage:
  python scripts/check.py          # Full check
  python scripts/check.py --fast   # Docs + hygiene only, ~1s
  python scripts/check.py --strict # Full check with strict mode
"""

import sys
import argparse
import os
from pathlib import Path

def check_docs(repo_root: Path) -> bool:
    """Verify docs structure: every folder has a README.md."""
    dirs_to_check = [
        repo_root / "docs",
        repo_root / "docs" / "design",
        repo_root / "docs" / "decisions",
        repo_root / "docs" / "roadmap",
    ]

    missing = []
    for d in dirs_to_check:
        readme = d / "README.md"
        if not readme.exists():
            missing.append(str(readme))

    if missing:
        print(f"❌ Missing README.md files:")
        for m in missing:
            print(f"   {m}")
        return False

    print(f"✓ Docs structure OK")
    return True

def check_hygiene(repo_root: Path) -> bool:
    """Basic repo hygiene: no temp files, etc."""
    # Check for common temp/build artifacts
    temp_patterns = ["*.swp", "*.swo", ".DS_Store", "*.bak"]

    issues = []
    for pattern in temp_patterns:
        found = list(repo_root.glob(f"**/{pattern}"))
        if found:
            issues.extend(found)

    if issues:
        print(f"❌ Found temp/backup files:")
        for f in issues[:10]:  # Show first 10
            print(f"   {f}")
        return False

    print(f"✓ Repo hygiene OK")
    return True

def main():
    parser = argparse.ArgumentParser(description="DualCEditor gate checker")
    parser.add_argument("--fast", action="store_true", help="Fast mode: docs + hygiene only")
    parser.add_argument("--strict", action="store_true", help="Strict mode: fail on warnings")

    args = parser.parse_args()

    repo_root = Path(__file__).parent.parent

    print("[gate] DualCEditor")

    all_ok = True
    all_ok &= check_docs(repo_root)
    all_ok &= check_hygiene(repo_root)

    if not all_ok:
        sys.exit(1)

    print("\n✓ All checks passed")
    sys.exit(0)

if __name__ == "__main__":
    main()
