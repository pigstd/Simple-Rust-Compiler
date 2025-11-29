#!/usr/bin/env python3
"""Run IRGen-with-runtime integration tests."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

PASS_MARK = "✅"
FAIL_MARK = "❌"
EXPECTED_EXIT_PREFIX = "// EXPECT_EXIT:"


def fail(msg: str) -> None:
    print(msg, file=sys.stderr)


def parse_expected_exit(source_text: str) -> int:
    for line in source_text.splitlines():
        stripped = line.strip()
        if stripped.startswith(EXPECTED_EXIT_PREFIX):
            try:
                return int(stripped[len(EXPECTED_EXIT_PREFIX) :].strip())
            except ValueError:
                fail(f"[WARN] Failed to parse EXPECT_EXIT line: {line}")
                return 0
    return 0


def discover_case(case_dir: Path) -> tuple[Path, Path, Path] | None:
    rx = next(case_dir.glob("*.rx"), None)
    inp = next(case_dir.glob("*.in"), None)
    out = next(case_dir.glob("*.out"), None)
    if not (rx and inp and out):
        return None
    return rx, inp, out


def compile_with_runtime(
    ir_text: str, runtime_ir: Path, clang_bin: str
) -> tuple[Path, tempfile.TemporaryDirectory[str]] | None:
    tmp = tempfile.TemporaryDirectory()
    tmp_path = Path(tmp.name)
    ir_path = tmp_path / "program.ll"
    exe_path = tmp_path / "a.out"
    ir_path.write_text(ir_text)
    compile_proc = subprocess.run(
        [clang_bin, str(ir_path), str(runtime_ir), "-O0", "-o", str(exe_path)],
        capture_output=True,
        text=True,
    )
    if compile_proc.returncode != 0:
        fail(f"{FAIL_MARK} clang failed:\n{compile_proc.stderr.strip()}")
        tmp.cleanup()
        return None
    return exe_path, tmp


def run_executable(
    exe_path: Path, stdin_data: str, expected_exit: int
) -> tuple[bool, str]:
    proc = subprocess.run(
        [str(exe_path)],
        input=stdin_data,
        text=True,
        capture_output=True,
    )
    if proc.returncode != expected_exit:
        fail(
            f"{FAIL_MARK} program exited with {proc.returncode}, expected {expected_exit}"
        )
        fail(proc.stderr.strip())
        return False, proc.stdout
    return True, proc.stdout


def run_case(
    driver: Path, runtime_ir: Path, clang_bin: str, case_dir: Path
) -> bool:
    files = discover_case(case_dir)
    if not files:
        fail(f"{FAIL_MARK} Missing .rx/.in/.out under {case_dir}")
        return False
    rx_path, in_path, out_path = files
    source_text = rx_path.read_text()
    expected_exit = parse_expected_exit(source_text)
    print(f"[CASE] {case_dir.name} (expect exit {expected_exit})")

    driver_proc = subprocess.run(
        [str(driver)],
        input=source_text,
        text=True,
        capture_output=True,
    )
    if driver_proc.returncode != 0:
        fail(f"{FAIL_MARK} driver failed:\n{driver_proc.stderr.strip()}")
        return False

    compile_result = compile_with_runtime(driver_proc.stdout, runtime_ir, clang_bin)
    if not compile_result:
        return False
    exe_path, tmp_dir = compile_result

    stdin_data = in_path.read_text()
    ok, stdout_text = run_executable(exe_path, stdin_data, expected_exit)
    tmp_dir.cleanup()
    if not ok:
        return False

    expected_output = out_path.read_text()
    if stdout_text.rstrip() != expected_output.rstrip():
        fail(f"{FAIL_MARK} output mismatch for {case_dir.name}")
        fail("--- expected ---")
        fail(expected_output)
        fail("--- actual ---")
        fail(stdout_text)
        return False

    print(f"   {PASS_MARK} {case_dir.name}")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description="Run IRGen-with-runtime tests")
    parser.add_argument(
        "-p",
        "--prefix",
        default="",
        help="Only run case directories whose names start with this prefix",
    )
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[2]
    bin_dir = repo_root / "test" / "build" / "IRGen_with_runtime"
    driver = bin_dir / "ir_program_driver_with_runtime"
    if not driver.exists():
        fail(f"[ERROR] Missing driver binary: {driver}")
        fail("Run `cmake --build build` to compile tests.")
        return 1

    runtime_ir = repo_root / "runtime" / "runtime.ll"
    if not runtime_ir.exists():
        fail(f"[ERROR] Missing runtime IR: {runtime_ir}")
        return 1

    clang_bin = shutil.which("clang")
    if not clang_bin:
        fail("[ERROR] clang not found in PATH")
        return 1

    tests_root = repo_root / "test" / "IRGen_with_runtime" / "tests"
    case_dirs = sorted([p for p in tests_root.iterdir() if p.is_dir()])
    if args.prefix:
        case_dirs = [p for p in case_dirs if p.name.startswith(args.prefix)]
    if not case_dirs:
        fail(f"[WARN] No test cases found under {tests_root}")
        return 1

    passed = 0
    for case in case_dirs:
        if run_case(driver, runtime_ir, clang_bin, case):
            passed += 1

    summary = f"Total: {len(case_dirs)}, Passed: {passed}, Failed: {len(case_dirs) - passed}"
    if passed == len(case_dirs):
        print(f"{PASS_MARK} {summary}")
        return 0
    print(f"{FAIL_MARK} {summary}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
