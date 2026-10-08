#!/usr/bin/env python3
"""Build and run the firmware host tests.

Each test_*.c in this directory is compiled into its own executable together with fakes.c and all of
Firmware/src/domain/*.c and Firmware/src/application/*.c (no Telink SDK, -DHOST_BUILD), then run.
One executable per test file gives every file fresh static state, since the application modules keep
theirs with no reset API. Needs a C compiler (cc, or $CC). Exits non-zero if anything fails to
compile or any check fails.
"""
import glob
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
SRC = os.path.join(ROOT, "Firmware", "src")


def main() -> int:
    tests = sorted(glob.glob(os.path.join(HERE, "test_*.c")))
    firmware = sorted(glob.glob(os.path.join(SRC, "domain", "*.c")) + glob.glob(os.path.join(SRC, "application", "*.c")))
    failed = 0
    with tempfile.TemporaryDirectory() as build:
        for test in tests:
            name = os.path.basename(test)
            exe = os.path.join(build, os.path.splitext(name)[0])
            compile_run = subprocess.run(
                [
                    os.environ.get("CC", "cc"),
                    "-std=gnu99", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter",
                    "-funsigned-char", "-DHOST_BUILD", "-I", SRC, "-I", HERE,
                    test, os.path.join(HERE, "fakes.c"), *firmware,
                    "-o", exe,
                ],
                capture_output=True, text=True,
            )
            if compile_run.returncode != 0:
                print(f"FAIL {name} (compile)")
                sys.stdout.write(compile_run.stdout + compile_run.stderr)
                failed += 1
                continue
            run = subprocess.run([exe], capture_output=True, text=True)
            summary = run.stdout.strip().splitlines()[-1:] or [""]
            if run.returncode == 0:
                print(f"PASS {name}: {summary[0]}")
            else:
                print(f"FAIL {name} (exit {run.returncode})")
                sys.stdout.write(run.stdout + run.stderr)
                failed += 1
    print(f"{len(tests) - failed}/{len(tests)} test files passed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
