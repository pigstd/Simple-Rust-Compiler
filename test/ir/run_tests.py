#!/usr/bin/env python3
"""Utility script to run the compiled IR builder tests."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


TEST_BINARIES = [
    "ir_builder_contract_test",
    "ir_builder_fixture_catalog",
    "ir_builder_fixture_runner",
]


def main() -> int:
    repo_root = Path(__file__).resolve().parents[2]
    bin_dir = repo_root / "test" / "build" / "ir"

    if not bin_dir.is_dir():
        print(f"[ERROR] Missing test binary directory: {bin_dir}", file=sys.stderr)
        print("Please run `cmake --build build` before executing this script.",
              file=sys.stderr)
        return 1

    for binary in TEST_BINARIES:
        target = bin_dir / binary
        if not target.exists():
            print(f"[ERROR] Test binary not found: {target}", file=sys.stderr)
            print("Make sure the project has been built.", file=sys.stderr)
            return 1

        print(f"[INFO] Running {binary} ...")
        try:
            subprocess.run([str(target)], check=True)
        except subprocess.CalledProcessError as exc:
            print(f"[FAIL] {binary} exited with status {exc.returncode}",
                  file=sys.stderr)
            return exc.returncode

    print("[OK] All IR builder tests passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
