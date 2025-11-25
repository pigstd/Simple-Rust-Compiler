#!/usr/bin/env python3
"""Run IRGen unit tests and sample program executions."""

from __future__ import annotations

import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

CXX_TEST_BINARIES = [
    "irgen_api_test",
    "function_context_default_state_test",
]

PROGRAM_SENTINEL = "IRGEN_NOT_IMPLEMENTED"
EXPECTED_EXIT_PREFIX = "// EXPECT_EXIT:"


def fail(msg: str) -> None:
    print(msg, file=sys.stderr)


def run_binary(target: Path) -> bool:
    print(f"[INFO] Running {target.name} ...")
    try:
        subprocess.run([str(target)], check=True, capture_output=True, text=True)
    except subprocess.CalledProcessError as exc:
        fail(f"[FAIL] {target.name} exited with status {exc.returncode}")
        if exc.stderr:
            fail(exc.stderr)
        return False
    return True


def run_cpp_tests(bin_dir: Path) -> bool:
    ok = True
    for binary in CXX_TEST_BINARIES:
        target = bin_dir / binary
        if not target.exists():
            fail(f"[ERROR] Test binary not found: {target}")
            return False
        ok = run_binary(target) and ok
    return ok


def parse_expected_exit(source_text: str) -> int:
    for line in source_text.splitlines():
        stripped = line.strip()
        if stripped.startswith(EXPECTED_EXIT_PREFIX):
            try:
                return int(stripped[len(EXPECTED_EXIT_PREFIX):].strip())
            except ValueError:
                fail(f"[WARN] Failed to parse EXPECT_EXIT line: {line}")
    return 0


def compile_and_run_ir(ir_text: str, clang_bin: str, expected_exit: int) -> bool:
    with tempfile.TemporaryDirectory() as tmpdir:
        exe_path = Path(tmpdir) / "a.out"
        compile_proc = subprocess.run(
            [clang_bin, "-x", "ir", "-", "-o", str(exe_path)],
            input=ir_text,
            text=True,
            capture_output=True,
        )
        if compile_proc.returncode != 0:
            fail("[FAIL] clang failed to compile IR snippet")
            fail(compile_proc.stderr)
            return False
        run_proc = subprocess.run([str(exe_path)])
        if run_proc.returncode != expected_exit:
            fail(
                f"[FAIL] Program exited with {run_proc.returncode}, "
                f"expected {expected_exit}"
            )
            return False
    return True


def run_program_case(driver: Path, clang_bin: str, case_path: Path) -> bool | None:
    source_text = case_path.read_text()
    expected_exit = parse_expected_exit(source_text)
    print(f"[CASE] {case_path.name} (expect exit {expected_exit})")
    proc = subprocess.run(
        [str(driver)],
        input=source_text,
        text=True,
        capture_output=True,
    )
    if proc.returncode != 0:
        fail(f"[FAIL] Driver failed for {case_path.name}")
        fail(proc.stderr)
        return False

    stdout = proc.stdout
    if PROGRAM_SENTINEL in stdout:
        print("[SKIP] IRGen not implemented yet; skipping execution.")
        return None

    return compile_and_run_ir(stdout, clang_bin, expected_exit)


def run_program_tests(bin_dir: Path, repo_root: Path) -> bool:
    driver = bin_dir / "ir_program_driver"
    if not driver.exists():
        fail(f"[WARN] Missing program driver binary: {driver}")
        return False

    program_dir = repo_root / "test" / "IRGen" / "programs"
    cases = sorted(program_dir.glob("*.rx"))
    if not cases:
        fail(f"[WARN] No program cases found under {program_dir}")
        return False

    clang_bin = shutil.which("clang")
    if clang_bin is None:
        fail("[WARN] clang not found in PATH; skipping program execution tests.")
        return False

    overall_ok = True
    skipped_all = True
    for case in cases:
        result = run_program_case(driver, clang_bin, case)
        if result is False:
            overall_ok = False
        if result is not None:
            skipped_all = False
    if skipped_all:
        print("[INFO] Program execution tests skipped (IRGen unavailable).")
    return overall_ok


def main() -> int:
    repo_root = Path(__file__).resolve().parents[2]
    bin_dir = repo_root / "test" / "build" / "IRGen"

    if not bin_dir.is_dir():
        fail(f"[ERROR] Missing test binary directory: {bin_dir}")
        fail("Please run `cmake --build build` before executing this script.")
        return 1

    ok = run_cpp_tests(bin_dir)
    prog_ok = run_program_tests(bin_dir, repo_root)
    if ok and prog_ok:
        print("[OK] All IRGen tests completed.")
        return 0
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
