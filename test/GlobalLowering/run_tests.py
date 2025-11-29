#!/usr/bin/env python3
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BUILD_BIN = ROOT / "test" / "build" / "GlobalLowering" / "global_lowering_driver_test"
SIZE_BIN = ROOT / "test" / "build" / "GlobalLowering" / "struct_size_after_lowering_test"
FIXTURE_DIR = ROOT / "test" / "GlobalLowering" / "fixtures"
SENTINEL = "GLOBAL_LOWERING_NOT_IMPLEMENTED"

CASES = [
    ("struct_and_array_const", FIXTURE_DIR / "struct_and_array.src", FIXTURE_DIR / "struct_and_array.ir"),
    ("const_chain_expands_values", FIXTURE_DIR / "const_chain.src", FIXTURE_DIR / "const_chain.ir"),
    ("nested_functions_unique_names", FIXTURE_DIR / "nested_functions.src", FIXTURE_DIR / "nested_functions.ir"),
    ("two_dim_array_const", FIXTURE_DIR / "two_dim_array.src", FIXTURE_DIR / "two_dim_array.ir"),
    ("function_with_params", FIXTURE_DIR / "function_with_params.src", FIXTURE_DIR / "function_with_params.ir"),
    ("nested_struct_dependency", FIXTURE_DIR / "nested_struct_dependency.src", FIXTURE_DIR / "nested_struct_dependency.ir"),
]

SIZE_CASES = [
    ("struct_size_check", FIXTURE_DIR / "struct_size_check.src", FIXTURE_DIR / "struct_size_check.size"),
]


def main() -> int:
    if not BUILD_BIN.exists():
        print(f"[skip] binary {BUILD_BIN} not found; build tests first.")
        return 0

    overall_ok = True
    for label, src, expected in CASES:
        print(f"\n=== {label} ===")
        if not src.exists() or not expected.exists():
            print(f"[error] missing fixture files for {label}")
            overall_ok = False
            continue

        with open(src, "r", encoding="utf-8") as fin:
            proc = subprocess.run([str(BUILD_BIN)], capture_output=True,
                                  text=True, stdin=fin)
        if SENTINEL in proc.stdout:
            print("global lowering not implemented yet; skipping comparison")
            continue
        if proc.returncode != 0:
            print(f"[error] binary returned {proc.returncode}:\n{proc.stderr}")
            overall_ok = False
            continue

        actual = proc.stdout.strip()
        expected_text = expected.read_text().strip()
        if actual == expected_text:
            print("[ok] output matches expected")
        else:
            print("[fail] output mismatch")
            print("--- expected ---")
            print(expected_text)
            print("--- actual ---")
            print(actual)
            overall_ok = False

    if SIZE_BIN.exists():
        for label, src, expected in SIZE_CASES:
            print(f"\n=== {label} (size) ===")
            if not src.exists() or not expected.exists():
                print(f"[error] missing fixture files for {label}")
                overall_ok = False
                continue
            with open(src, "r", encoding="utf-8") as fin:
                proc = subprocess.run([str(SIZE_BIN)], capture_output=True,
                                      text=True, stdin=fin)
            if proc.returncode != 0:
                print(f"[error] binary returned {proc.returncode}:\n{proc.stderr}")
                overall_ok = False
                continue
            actual = proc.stdout.strip()
            expected_text = expected.read_text().strip()
            if actual == expected_text:
                print("[ok] struct sizes match expected")
            else:
                print("[fail] struct size mismatch")
                print("--- expected ---")
                print(expected_text)
                print("--- actual ---")
                print(actual)
                overall_ok = False
    else:
        print(f"[skip] binary {SIZE_BIN} not found; build tests to enable struct size checks.")
    return 0 if overall_ok else 1


if __name__ == "__main__":
    sys.exit(main())
