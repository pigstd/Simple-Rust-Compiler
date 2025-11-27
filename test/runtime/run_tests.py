#!/usr/bin/env python3
"""Run runtime integration tests by compiling programs with the runtime driver."""

from __future__ import annotations

import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DRIVER = ROOT / "test" / "build" / "runtime" / "runtime_driver"
RUNTIME_LL = ROOT / "runtime" / "runtime.ll"
CASE_ROOT = Path(__file__).resolve().parent / "tests"
TMP_BASE = ROOT / "tmp" / "runtime"
PASS_MARK = "✅"
FAIL_MARK = "❌"


def fail(msg: str) -> None:
    print(msg, file=sys.stderr)


def compile_and_run(case_dir: Path) -> bool:
    rx_path = next(case_dir.glob("*.rx"), None)
    out_path = next(case_dir.glob("*.out"), None)
    in_path = next(case_dir.glob("*.in"), None)
    if not rx_path or not out_path or not in_path:
        fail(f"[ERROR] Missing .rx/.in/.out under {case_dir}")
        return False

    rx_source = rx_path.read_text()
    proc = subprocess.run(
        [str(DRIVER)],
        input=rx_source,
        text=True,
        capture_output=True,
    )
    if proc.returncode != 0:
        fail(f"[ERROR] runtime_driver failed for {case_dir.name}")
        fail(proc.stderr)
        return False

    if not RUNTIME_LL.exists():
        fail(f"[ERROR] runtime.ll missing at {RUNTIME_LL}")
        return False

    TMP_BASE.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=TMP_BASE) as tmpdir:
        ir_path = Path(tmpdir) / "program.ll"
        exe_path = Path(tmpdir) / "a.out"
        ir_path.write_text(proc.stdout)

        compile_proc = subprocess.run(
            ["clang", str(ir_path), str(RUNTIME_LL), "-o", str(exe_path)],
            capture_output=True,
            text=True,
        )
        if compile_proc.returncode != 0:
            fail(f"[ERROR] clang failed for {case_dir.name}")
            fail(compile_proc.stderr)
            return False

        stdin_data = in_path.read_text()
        run_proc = subprocess.run(
            [str(exe_path)],
            input=stdin_data,
            text=True,
            capture_output=True,
        )
        expected_out = out_path.read_text()
        if run_proc.stdout != expected_out:
            fail(f"[FAIL] Output mismatch for {case_dir.name}")
            fail(f"--- expected ---\n{expected_out}--- actual ---\n{run_proc.stdout}")
            return False
        if run_proc.returncode != 0:
            fail(f"[FAIL] Program exited with {run_proc.returncode} (expected 0)")
            return False
    print(f"{PASS_MARK} {case_dir.name}")
    return True


def main() -> int:
    if not DRIVER.exists():
        fail(f"[ERROR] Missing driver binary: {DRIVER}")
        fail("Please run `cmake --build build` first.")
        return 1
    if not shutil.which("clang"):
        fail("[ERROR] clang not found in PATH")
        return 1
    if not CASE_ROOT.exists():
        fail(f"[ERROR] Test cases directory missing: {CASE_ROOT}")
        return 1

    case_dirs = sorted([p for p in CASE_ROOT.iterdir() if p.is_dir()])
    if not case_dirs:
        fail(f"[ERROR] No test cases found under {CASE_ROOT}")
        return 1

    ok = True
    for case in case_dirs:
        ok = compile_and_run(case) and ok
    if ok:
        print(f"{PASS_MARK} [OK] All runtime tests passed.")
        return 0
    print(f"{FAIL_MARK} Runtime tests failed.")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
